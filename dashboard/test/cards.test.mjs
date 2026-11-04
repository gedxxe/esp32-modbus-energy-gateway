import test from "node:test";
import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { JSDOM, VirtualConsole } from "jsdom";
import { fixture } from "./fixture.mjs";
const html = await readFile(
  new URL("../dist/index.html", import.meta.url),
  "utf8",
);
const wait = (ms = 70) => new Promise((r) => setTimeout(r, ms));
function setInput(w, input, value) {
  Object.getOwnPropertyDescriptor(
    w.HTMLInputElement.prototype,
    "value",
  ).set.call(input, value);
  input._valueTracker?.setValue("");
  input.dispatchEvent(new w.Event("input", { bubbles: true }));
}
async function mount(getState, onCommand = () => {}, before = () => {}) {
  const errors = [];
  const vc = new VirtualConsole();
  vc.on("jsdomError", (e) => errors.push(e.message));
  const dom = new JSDOM(html, {
    url: "http://192.168.4.1/",
    runScripts: "dangerously",
    virtualConsole: vc,
    beforeParse(w) {
      w.HTMLDialogElement.prototype.showModal = function () {
        this.open = true;
      };
      w.fetch = async (path, options) => ({
        ok: true,
        json: async () =>
          path === "/api/status"
            ? getState()
            : onCommand(JSON.parse(options.body)),
        text: async () => "",
      });
      before(w);
    },
  });
  const w = dom.window,
    d = w.document;
  await wait();
  setInput(w, d.querySelector("input"), "qa");
  await wait();
  d.querySelector("form").dispatchEvent(
    new w.Event("submit", { bubbles: true, cancelable: true }),
  );
  await wait();
  return { dom, w, d, errors };
}
test("production cards keep three node identities, RX diagnostics and data isolated", async () => {
  const state = fixture();
  state.nodes = Array.from({ length: 3 }, (_, i) => ({
    ...structuredClone(state.nodes[0]),
    index: i,
    id: i + 2,
    name: `Panel ${i + 2}`,
    accepted: 21 + i,
    lastSequence: 28 + i,
    live: { ...state.nodes[0].live, kwh: 100 + i },
  }));
  state.nodes[1].receiveAge = 30000;
  state.nodes[2] = {
    ...state.nodes[2],
    session: null,
    candidate: "candidate",
    hasLive: false,
    lastType: 1,
    lastSequence: 1,
    accepted: 1,
  };
  const { dom, d, errors } = await mount(() => state);
  try {
    assert.equal(d.querySelectorAll(".node-card").length, 3);
    const cards = [...d.querySelectorAll(".node-card")];
    assert.ok(cards[0].textContent.includes("100 kWh"));
    assert.ok(cards[1].textContent.includes("Stale data"));
    assert.ok(cards[2].textContent.includes("Confirming session"));
    assert.match(cards[0].textContent, /21/);
    assert.match(cards[0].textContent, /TELEMETRY \/ #28/);
    assert.match(cards[2].textContent, /HELLO \/ #1/);
    assert.ok(
      d.querySelector(".add-node-card").textContent.includes("Add node"),
    );
    assert.ok(d.body.textContent.includes("saved in gateway flash"));
    assert.deepEqual(errors, []);
  } finally {
    dom.window.close();
  }
});
test("Add node card validates duplicate IDs and waits for verified flash-save status", async () => {
  let state = fixture();
  const commands = [];
  const { dom, w, d, errors } = await mount(
    () => state,
    (c) => {
      commands.push(c);
      state = {
        ...state,
        revision: 2,
        nodes: [
          ...state.nodes,
          {
            ...structuredClone(state.nodes[0]),
            ...c.node,
            index: c.index,
            key: undefined,
            hasKey: true,
            revision: 1,
            session: null,
            hasLive: false,
            packetAge: null,
          },
        ],
        results: [
          {
            id: 99,
            ok: true,
            message:
              "Saved and queued for radio apply; sessions will reconfirm",
          },
        ],
      };
      return { id: 99 };
    },
  );
  try {
    d.querySelector(".add-node-card").click();
    await wait();
    const dialog = d.querySelector("dialog[open]");
    assert.ok(dialog);
    const fields = [...dialog.querySelectorAll("label")];
    const input = (label) =>
      fields.find((f) => f.textContent.includes(label)).querySelector("input");
    assert.equal(input("Node ID").value, "3");
    setInput(w, input("Node ID"), "2");
    await wait();
    assert.ok(dialog.textContent.includes("already registered"));
    assert.ok(dialog.querySelector("button.primary").disabled);
    setInput(w, input("Node ID"), "3");
    setInput(w, input("Name"), "Panel B");
    setInput(w, input("New pairing key"), "ab".repeat(32));
    await wait();
    dialog.querySelector("input[type=checkbox]").click();
    await wait();
    dialog
      .querySelector("form")
      .dispatchEvent(
        new w.Event("submit", { bubbles: true, cancelable: true }),
      );
    await wait();
    assert.equal(commands.length, 1);
    assert.equal(commands[0].action, "saveNode");
    assert.equal(commands[0].node.id, 3);
    assert.equal(commands[0].node.enabled, true);
    assert.equal(d.querySelectorAll(".node-card").length, 1);
    assert.ok(d.body.textContent.includes("queued"));
    assert.equal(w.localStorage.length, 0);
    assert.equal(w.sessionStorage.length, 0);
    await wait(3100);
    assert.equal(d.querySelectorAll(".node-card").length, 2);
    assert.ok(d.body.textContent.includes("Saved and queued for radio apply"));
    assert.ok(!d.body.textContent.includes("ab".repeat(32)));
    assert.deepEqual(errors, []);
  } finally {
    dom.window.close();
  }
});
test("data ages continue increasing if status polling stalls", async () => {
  const state = fixture();
  let now = 0,
    tick;
  const { dom, w, d } = await mount(
    () => state,
    () => {},
    (w) => {
      w.performance.now = () => now;
      w.setInterval = (fn, ms) => {
        if (ms === 1000) tick = fn;
        return ms;
      };
      w.clearInterval = () => {};
    },
  );
  try {
    assert.ok(d.querySelector(".node-card").textContent.includes("Fresh data"));
    now = 16000;
    tick();
    await wait();
    assert.ok(d.querySelector(".node-card").textContent.includes("Stale data"));
    assert.ok(d.querySelector(".energy").textContent.includes("kWh"));
    assert.notEqual(d.querySelector(".energy").textContent.trim(), "— kWh");
  } finally {
    dom.window.close();
  }
});
test("open form keeps its save revision while newer inventory arrives", async () => {
  let state = fixture(),
    poll;
  const commands = [];
  const { dom, w, d } = await mount(
    () => state,
    (c) => {
      commands.push(c);
      return { id: 100 };
    },
    (w) => {
      w.setInterval = (fn, ms) => {
        if (ms === 3000) poll = fn;
        return ms;
      };
      w.clearInterval = () => {};
    },
  );
  try {
    d.querySelector(".node-edit").click();
    await wait();
    state = { ...state, revision: 2 };
    await poll();
    await wait();
    const form = d.querySelector("dialog form");
    form.dispatchEvent(
      new w.Event("submit", { bubbles: true, cancelable: true }),
    );
    await wait();
    assert.equal(commands[0].expected, 1);
  } finally {
    dom.window.close();
  }
});
