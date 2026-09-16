export function createWasiImports(getMemory) {
  const view = () => new DataView(getMemory().buffer);
  const implementations = {
    args_get: () => 0,
    args_sizes_get: (argc, size) => { view().setUint32(argc, 0, true); view().setUint32(size, 0, true); return 0; },
    environ_get: () => 0,
    environ_sizes_get: (count, size) => { view().setUint32(count, 0, true); view().setUint32(size, 0, true); return 0; },
    clock_time_get: (_clock, _precision, time) => { view().setBigUint64(time, BigInt(Date.now()) * 1000000n, true); return 0; },
    random_get: (pointer, length) => {
      const bytes = new Uint8Array(getMemory().buffer, pointer, length);
      for (let offset = 0; offset < length; offset += 65536)
        crypto.getRandomValues(bytes.subarray(offset, Math.min(offset + 65536, length)));
      return 0;
    },
    fd_prestat_get: () => 8,
    fd_write: (fd, iovs, count, writtenPointer) => {
      const memory = getMemory(), dataView = new DataView(memory.buffer);
      let written = 0, text = "";
      for (let i = 0; i < count; i++) {
        const pointer = dataView.getUint32(iovs + i * 8, true);
        const length = dataView.getUint32(iovs + i * 8 + 4, true);
        text += new TextDecoder().decode(new Uint8Array(memory.buffer, pointer, length));
        written += length;
      }
      dataView.setUint32(writtenPointer, written, true);
      (fd === 2 ? console.error : console.log)(text.trimEnd());
      return 0;
    },
    proc_exit: code => { throw new Error(`WASI exited with status ${code}`); },
  };
  return new Proxy(implementations, { get: (target, property) => target[property] ?? (() => 52) });
}
