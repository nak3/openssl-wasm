import { createWasiImports } from "./wasi.js";

const encoder = new TextEncoder();
const decoder = new TextDecoder();
let wasm, memory, handshake;
let visibleEvents = 0;
let timer;

const runtime = document.querySelector("#runtime");
const eventsElement = document.querySelector("#events");
const packet = document.querySelector("#packet");

function readCString(pointer) {
  const bytes = new Uint8Array(memory.buffer);
  let end = pointer;
  while (bytes[end]) end++;
  return decoder.decode(bytes.subarray(pointer, end));
}

function allocate(value) {
  const bytes = encoder.encode(value);
  const pointer = wasm.tls_lab_alloc(bytes.length);
  new Uint8Array(memory.buffer, pointer, bytes.length).set(bytes);
  return { pointer, length: bytes.length };
}

function prepare(cert, key) {
  const c = allocate(cert), k = allocate(key);
  try {
    wasm.tls_lab_run(c.pointer, c.length, k.pointer, k.length);
    handshake = JSON.parse(readCString(wasm.tls_lab_result()));
    if (!handshake.ok) throw new Error(handshake.error);
  } finally {
    wasm.tls_lab_free(c.pointer);
    wasm.tls_lab_free(k.pointer);
  }
}

function showNext() {
  if (!handshake || visibleEvents >= handshake.events.length) return false;
  const event = handshake.events[visibleEvents++];
  document.querySelector("#empty").hidden = true;
  const item = document.createElement("li");
  const fromClient = event.direction === "client-to-server";
  item.className = `event ${fromClient ? "client" : "server"}`;
  item.innerHTML = `<span class="event-number">${String(visibleEvents).padStart(2, "0")}</span>
    <span class="event-direction">${fromClient ? "CLIENT → SERVER" : "SERVER → CLIENT"}</span>
    <strong>${event.label}</strong><span class="event-meta">${event.record} · ${event.bytes} B</span>`;
  eventsElement.append(item);
  packet.hidden = false;
  packet.className = `packet ${fromClient ? "to-server" : "to-client"}`;
  packet.textContent = event.label;
  document.querySelector(fromClient ? "#client-state" : "#server-state").textContent = event.state;
  document.querySelector("#step-counter").textContent = `${visibleEvents} / ${handshake.events.length}`;
  if (visibleEvents === handshake.events.length) {
    document.querySelector("#negotiated").textContent = `${handshake.version} · ${handshake.cipher} · ${handshake.applicationData}`;
    document.querySelector("#client-state").textContent = "Connected";
    document.querySelector("#server-state").textContent = "Connected";
  }
  return true;
}

function reset() {
  clearInterval(timer);
  visibleEvents = 0;
  eventsElement.innerHTML = "";
  packet.hidden = true;
  document.querySelector("#empty").hidden = false;
  document.querySelector("#step-counter").textContent = `0 / ${handshake?.events.length || 0}`;
  document.querySelector("#client-state").textContent = "Ready";
  document.querySelector("#server-state").textContent = "Ready";
  document.querySelector("#negotiated").textContent = "Not negotiated";
}

document.querySelector("#next").addEventListener("click", showNext);
document.querySelector("#reset").addEventListener("click", reset);
document.querySelector("#play").addEventListener("click", () => {
  reset();
  timer = setInterval(() => { if (!showNext()) clearInterval(timer); }, 650);
});

async function initialize() {
  const [cert, key] = await Promise.all([
    fetch("./demo-cert.pem").then(r => r.text()),
    fetch("./demo-key.pem").then(r => r.text()),
  ]);
  const imports = createWasiImports(() => memory);
  const module = await WebAssembly.instantiateStreaming(fetch("./tls-lab.wasm"), {
    wasi_snapshot_preview1: imports,
  });
  wasm = module.instance.exports;
  memory = wasm.memory;
  wasm._initialize?.();
  prepare(cert, key);
  runtime.textContent = "LibreSSL WebAssembly ready";
  runtime.classList.add("ready");
  document.querySelectorAll("button").forEach(button => button.disabled = false);
  document.querySelector("#step-counter").textContent = `0 / ${handshake.events.length}`;
}

initialize().catch(error => {
  runtime.textContent = error.message;
  console.error(error);
});
