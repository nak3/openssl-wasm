import { createWasiImports } from "./wasi.js";

const encoder = new TextEncoder();
const decoder = new TextDecoder();
let wasm;
let memory;

const certificate = document.querySelector("#certificate");
const caCertificate = document.querySelector("#ca-certificate");
const status = document.querySelector("#status");
const results = document.querySelector("#results");
const verifyResult = document.querySelector("#verify-result");

function readCString(pointer) {
  const bytes = new Uint8Array(memory.buffer);
  let end = pointer;
  while (bytes[end] !== 0) end++;
  return decoder.decode(bytes.subarray(pointer, end));
}

function withWasmString(value, callback) {
  const bytes = encoder.encode(value);
  const pointer = wasm.lab_alloc(bytes.length);
  new Uint8Array(memory.buffer, pointer, bytes.length).set(bytes);
  try {
    return callback(pointer, bytes.length);
  } finally {
    wasm.lab_free(pointer);
  }
}

function getResult() {
  return JSON.parse(readCString(wasm.lab_result()));
}

function renderCertificate(data) {
  if (!data.ok) throw new Error(data.error);
  const fields = [
    ["Subject", data.subject],
    ["Issuer", data.issuer],
    ["Valid from", data.notBefore],
    ["Valid until", data.notAfter],
    ["Public key", `${data.publicKey.algorithm} · ${data.publicKey.bits} bits`],
    ["Signature", data.signatureAlgorithm],
    ["Subject alternative names", data.sans.length ? data.sans.join(", ") : "None"],
    ["SHA-256 fingerprint", data.fingerprint],
  ];
  results.innerHTML = fields.map(([label, value]) => `
    <div class="result-row">
      <dt>${label}</dt><dd>${escapeHtml(value)}</dd>
    </div>`).join("");
  document.querySelector("#empty-state").hidden = true;
  results.hidden = false;
}

function escapeHtml(value) {
  const span = document.createElement("span");
  span.textContent = value;
  return span.innerHTML;
}

function parse() {
  if (!certificate.value.trim()) return;
  try {
    withWasmString(certificate.value, (pointer, length) =>
      wasm.lab_parse_certificate(pointer, length));
    renderCertificate(getResult());
    status.textContent = "Parsed locally with LibreSSL";
    status.dataset.kind = "success";
  } catch (error) {
    status.textContent = error.message;
    status.dataset.kind = "error";
  }
}

function verify() {
  if (!certificate.value.trim() || !caCertificate.value.trim()) {
    verifyResult.textContent = "Add both a certificate and a CA certificate.";
    verifyResult.dataset.kind = "error";
    return;
  }
  withWasmString(certificate.value, (certPointer, certLength) =>
    withWasmString(caCertificate.value, (caPointer, caLength) =>
      wasm.lab_verify_certificate(certPointer, certLength, caPointer, caLength)));
  const data = getResult();
  verifyResult.textContent = data.message || data.error;
  verifyResult.dataset.kind = data.verified ? "success" : "error";
}

function wireDropZone(element) {
  element.addEventListener("dragover", (event) => {
    event.preventDefault();
    element.closest(".input-card").classList.add("dragging");
  });
  element.addEventListener("dragleave", () =>
    element.closest(".input-card").classList.remove("dragging"));
  element.addEventListener("drop", async (event) => {
    event.preventDefault();
    element.closest(".input-card").classList.remove("dragging");
    const file = event.dataTransfer.files[0];
    if (file) {
      element.value = await file.text();
      if (element === certificate) parse();
    }
  });
}

async function initialize() {
  let instance;
  const imports = createWasiImports(() => memory);
  const response = await fetch("./libressl-lab.wasm");
  const module = await WebAssembly.instantiateStreaming(response, {
    wasi_snapshot_preview1: imports,
  });
  instance = module.instance;
  wasm = instance.exports;
  memory = wasm.memory;
  wasm._initialize?.();
  status.textContent = "LibreSSL WebAssembly ready";
  status.dataset.kind = "success";
  document.querySelectorAll("button").forEach((button) => button.disabled = false);
}

document.querySelector("#parse").addEventListener("click", parse);
document.querySelector("#verify").addEventListener("click", verify);
document.querySelector("#clear").addEventListener("click", () => {
  certificate.value = "";
  results.hidden = true;
  document.querySelector("#empty-state").hidden = false;
});
document.querySelector("#sample").addEventListener("click", async () => {
  const pem = await fetch("./sample.pem").then((response) => response.text());
  certificate.value = pem;
  caCertificate.value = pem;
  parse();
});
wireDropZone(certificate);
wireDropZone(caCertificate);

initialize().catch((error) => {
  status.textContent = `Could not load WebAssembly: ${error.message}`;
  status.dataset.kind = "error";
  console.error(error);
});
