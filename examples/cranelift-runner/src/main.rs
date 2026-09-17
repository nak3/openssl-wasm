use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use std::time::{Duration, Instant};

use anyhow::{bail, Context, Result};
use wasmtime::{
    Config, Engine, Linker, Module, OptLevel, Store, StoreLimits, StoreLimitsBuilder, Strategy,
};
use wasmtime_wasi::preview1::{self, WasiP1Ctx};
use wasmtime_wasi::WasiCtxBuilder;

const DEFAULT_ROUNDS: u32 = 10_000;
const DEFAULT_CALLS: u32 = 20;
const MEMORY_LIMIT: usize = 32 * 1024 * 1024;

struct Options {
    module: PathBuf,
    artifact: PathBuf,
    rounds: u32,
    calls: u32,
    fuel: Option<u64>,
}

struct HostState {
    limits: StoreLimits,
    wasi: WasiP1Ctx,
}

struct Measurement {
    result: u64,
    instantiate: Duration,
    execute: Duration,
}

fn main() -> Result<()> {
    let options = parse_args()?;
    let mut config = Config::new();
    config.strategy(Strategy::Cranelift);
    config.cranelift_opt_level(OptLevel::Speed);
    config.consume_fuel(options.fuel.is_some());
    let engine = Engine::new(&config).context("failed to create the Cranelift engine")?;

    let started = Instant::now();
    let jit_module = Module::from_file(&engine, &options.module)
        .with_context(|| format!("failed to compile {}", options.module.display()))?;
    let compile_time = started.elapsed();

    let started = Instant::now();
    let serialized = jit_module
        .serialize()
        .context("failed to serialize the compiled module")?;
    fs::write(&options.artifact, &serialized)
        .with_context(|| format!("failed to write {}", options.artifact.display()))?;
    let serialize_time = started.elapsed();

    let jit = measure(&engine, &jit_module, &options)?;

    let started = Instant::now();
    // The artifact was produced above by this process with the same Engine configuration.
    // Never deserialize an untrusted precompiled artifact.
    let aot_module = unsafe { Module::deserialize(&engine, &serialized) }
        .context("failed to load the precompiled module")?;
    let load_time = started.elapsed();
    let aot = measure(&engine, &aot_module, &options)?;

    if jit.result != aot.result {
        bail!(
            "JIT and AOT results differ: {:#018x} != {:#018x}",
            jit.result,
            aot.result
        );
    }

    println!("LibreSSL SHA-256 through Wasmtime/Cranelift");
    println!("  module:       {}", options.module.display());
    println!(
        "  artifact:     {} ({} bytes)",
        options.artifact.display(),
        serialized.len()
    );
    println!(
        "  workload:     {} calls x {} SHA-256 rounds",
        options.calls, options.rounds
    );
    println!("  result:       {:#018x}", jit.result);
    println!();
    println!("startup");
    println!("  Wasm compile: {}", format_duration(compile_time));
    println!("  AOT serialize:{}", format_duration(serialize_time));
    println!("  AOT load:     {}", format_duration(load_time));
    println!();
    println!("execution");
    println!("  JIT instantiate: {}", format_duration(jit.instantiate));
    println!("  JIT calls:       {}", format_duration(jit.execute));
    println!("  AOT instantiate: {}", format_duration(aot.instantiate));
    println!("  AOT calls:       {}", format_duration(aot.execute));
    println!();
    println!("AOT avoids Wasm-to-native compilation; both paths execute Cranelift native code.");
    Ok(())
}

fn measure(engine: &Engine, module: &Module, options: &Options) -> Result<Measurement> {
    let limits = StoreLimitsBuilder::new()
        .memory_size(MEMORY_LIMIT)
        .instances(1)
        .memories(1)
        .build();
    let mut store = Store::new(
        engine,
        HostState {
            limits,
            // Deliberately inherit no stdio, environment variables, arguments,
            // directories, or sockets from the host.
            wasi: WasiCtxBuilder::new().build_p1(),
        },
    );
    store.limiter(|state| &mut state.limits);
    if let Some(fuel) = options.fuel {
        store
            .set_fuel(fuel)
            .context("failed to set the fuel limit")?;
    }

    let started = Instant::now();
    let mut linker = Linker::new(engine);
    preview1::add_to_linker_sync(&mut linker, |state: &mut HostState| &mut state.wasi)
        .context("failed to configure WASI Preview 1")?;
    let instance = linker
        .instantiate(&mut store, module)
        .context("failed to instantiate module")?;
    let function = instance
        .get_typed_func::<u32, u64>(&mut store, "libressl_sha256_benchmark")
        .context("module does not export libressl_sha256_benchmark(u32) -> u64")?;
    let instantiate = started.elapsed();

    // Warm up host/guest call paths before timing.
    let mut result = function
        .call(&mut store, options.rounds)
        .context("warm-up call failed")?;
    let started = Instant::now();
    for _ in 0..options.calls {
        result = function
            .call(&mut store, options.rounds)
            .context("benchmark call failed")?;
    }
    let execute = started.elapsed();

    Ok(Measurement {
        result,
        instantiate,
        execute,
    })
}

fn parse_args() -> Result<Options> {
    let mut args = env::args_os().skip(1);
    let mut module = None;
    let mut artifact = None;
    let mut rounds = DEFAULT_ROUNDS;
    let mut calls = DEFAULT_CALLS;
    let mut fuel = None;

    while let Some(arg) = args.next() {
        match arg.to_str() {
            Some("--artifact") => {
                artifact = Some(PathBuf::from(next_value(&mut args, "--artifact")?))
            }
            Some("--rounds") => {
                rounds = parse_number(next_value(&mut args, "--rounds")?, "--rounds")?
            }
            Some("--calls") => calls = parse_number(next_value(&mut args, "--calls")?, "--calls")?,
            Some("--fuel") => {
                fuel = Some(parse_number(next_value(&mut args, "--fuel")?, "--fuel")?)
            }
            Some("-h" | "--help") => {
                print_usage();
                std::process::exit(0);
            }
            Some(value) if value.starts_with('-') => bail!("unknown option: {value}"),
            _ if module.is_none() => module = Some(PathBuf::from(arg)),
            _ => bail!("only one Wasm module may be specified"),
        }
    }

    let module = module.unwrap_or_else(default_module_path);
    let artifact = artifact.unwrap_or_else(|| module.with_extension("cwasm"));
    if rounds == 0 || calls == 0 {
        bail!("--rounds and --calls must both be greater than zero");
    }
    Ok(Options {
        module,
        artifact,
        rounds,
        calls,
        fuel,
    })
}

fn next_value(
    args: &mut impl Iterator<Item = std::ffi::OsString>,
    option: &str,
) -> Result<std::ffi::OsString> {
    args.next()
        .with_context(|| format!("{option} requires a value"))
}

fn parse_number<T>(value: std::ffi::OsString, option: &str) -> Result<T>
where
    T: std::str::FromStr,
    T::Err: std::error::Error + Send + Sync + 'static,
{
    value
        .to_str()
        .with_context(|| format!("{option} must be valid UTF-8"))?
        .parse()
        .with_context(|| format!("invalid value for {option}"))
}

fn default_module_path() -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR")).join("benchmark.wasm")
}

fn format_duration(duration: Duration) -> String {
    if duration.as_secs() > 0 {
        format!("{:>10.3} s", duration.as_secs_f64())
    } else if duration.as_millis() > 0 {
        format!("{:>10.3} ms", duration.as_secs_f64() * 1_000.0)
    } else {
        format!("{:>10.3} us", duration.as_secs_f64() * 1_000_000.0)
    }
}

fn print_usage() {
    println!("Usage: libressl-cranelift-runner [MODULE] [OPTIONS]");
    println!();
    println!("Options:");
    println!("  --artifact PATH  write the Cranelift AOT artifact here");
    println!("  --rounds N       SHA-256 operations per guest call (default: {DEFAULT_ROUNDS})");
    println!("  --calls N        timed guest calls per path (default: {DEFAULT_CALLS})");
    println!("  --fuel N         cap guest instructions per JIT/AOT run");
    println!("  -h, --help       show this help");
}
