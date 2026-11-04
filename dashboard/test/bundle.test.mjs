import test from "node:test";
import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { gunzipSync } from "node:zlib";
import { JSDOM, VirtualConsole } from "jsdom";
import { fixture } from "./fixture.mjs";
const html = await readFile(
  new URL("../dist/index.html", import.meta.url),
  "utf8",
);
const wait = () => new Promise((r) => setTimeout(r, 70));
test("built UI hides missing energy and retains the last valid stale reading", async () => {
  for (const mode of ["missing", "stale"]) {
    const state = fixture();
    if (mode === "missing") {
      state.nodes[0].live.flags = 8;
      state.nodes[0].live.kwh = null;
    } else state.nodes[0].receiveAge = 16000;
    const dom = new JSDOM(html, {
      url: "http://192.168.4.1/",
      runScripts: "dangerously",
      beforeParse(w) {
        w.fetch = async () => ({ ok: true, json: async () => state });
      },
    });
    try {
      const w = dom.window,
        d = w.document;
      await wait();
      const input = d.querySelector("input");
      Object.getOwnPropertyDescriptor(
        w.HTMLInputElement.prototype,
        "value",
      ).set.call(input, "qa");
      input._valueTracker?.setValue("");
      input.dispatchEvent(new w.Event("input", { bubbles: true }));
      await wait();
      d.querySelector("form").dispatchEvent(
        new w.Event("submit", { bubbles: true, cancelable: true }),
      );
      await wait();
      assert.equal(d.querySelector(".energy").textContent.trim(), mode === "missing" ? "— kWh" : "1,234.567 kWh");
      assert.ok(d.body.textContent.includes("Stale data"));
    } finally {
      dom.window.close();
    }
  }
});
test("actual production gzip matches HTML and stays below 80 KiB", async () => {
  const gz = await readFile(new URL("../dist/index.html.gz", import.meta.url));
  assert.equal(gunzipSync(gz).toString(), html);
  assert.ok(gz.length < 81920);
  const staticDom = new JSDOM(html);
  assert.equal(
    staticDom.window.document.querySelectorAll(
      'script[src],link[href],img[src^="http"]',
    ).length,
    0,
  );
  staticDom.window.close();
  assert.ok(!html.includes("<script src="));
  const assets = await readFile(
    new URL("../../firmware/web/assets.h", import.meta.url),
    "utf8",
  );
  const bytes = assets
    .match(/PROGMEM=\{([^}]+)\}/)[1]
    .split(",")
    .map(Number);
  assert.deepEqual(Buffer.from(bytes), gz);
});
test("built React UI: authentication, tabs, missing energy, command queue, and key hygiene", async () => {
  const errors = [],
    calls = [];
  let state = fixture();
  const vc = new VirtualConsole();
  vc.on("jsdomError", (e) => errors.push(e.message));
  const dom = new JSDOM(html, {
    url: "http://192.168.4.1/",
    runScripts: "dangerously",
    virtualConsole: vc,
    pretendToBeVisual: true,
    beforeParse(w) {
      w.HTMLDialogElement.prototype.showModal = function () {
        this.open = true;
      };
      w.fetch = async (path, options) => {
        calls.push({ path, options });
        return {
          ok: true,
          json: async () =>
            path === "/api/status"
              ? state
              : path === "/api/archives"
                ? { archives: [] }
                : { id: 99 },
          text: async () => "",
        };
      };
    },
  });
  const w = dom.window,
    d = w.document;
  await wait();
  assert.ok(d.body.textContent.includes("LoRa power monitoring"));
  const input = d.querySelector("input");
  Object.getOwnPropertyDescriptor(
    w.HTMLInputElement.prototype,
    "value",
  ).set.call(input, "boot-code");
  input.dispatchEvent(new w.Event("input", { bubbles: true })); // submit form reads state updated below via change-compatible native event
  // React 18 needs value tracker reset to observe programmatic input.
  input._valueTracker?.setValue("");
  input.dispatchEvent(new w.Event("input", { bubbles: true }));
  await wait();
  d.querySelector("form").dispatchEvent(
    new w.Event("submit", { bubbles: true, cancelable: true }),
  );
  await wait();
  assert.ok(d.body.textContent.includes("Node activity"));
  assert.equal(
    d.querySelector(".energy").textContent.replace(/[^0-9]/g, ""),
    "1234567",
  );
  assert.equal(w.localStorage.length, 0);
  assert.equal(w.sessionStorage.length, 0);
  assert.ok(!d.body.textContent.includes("boot-code"));
  const click = (text) => {
    const b = [...d.querySelectorAll("button")].find(
      (b) =>
        b.textContent.includes(text) ||
        b.getAttribute("aria-label")?.includes(text),
    );
    assert.ok(b, `button ${text}`);
    b.click();
  };
  click("Node inventory");
  await wait();
  click("Edit");
  await wait();
  assert.ok(d.querySelector("dialog[open]"));
  const key = d.querySelector("input[pattern]");
  assert.equal(key.value, "");
  click("Cancel");
  await wait();
  click("Request live");
  await wait();
  assert.ok(
    calls.some(
      (c) =>
        c.path === "/api/command" &&
        JSON.parse(c.options.body).action === "live",
    ),
  );
  assert.ok(d.body.textContent.includes("queued"));
  click("Gateway & radio");
  await wait();
  assert.ok(d.body.textContent.includes("One SX1262"));
  click("Diagnostics");
  await wait();
  assert.ok(d.body.textContent.includes("LoRa airtime and refresh policy"));
  click("EN / ID");
  await wait();
  assert.ok(d.body.textContent.includes("Diagnostik"));
  assert.deepEqual(errors, []);
  dom.window.close();
});
