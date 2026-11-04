import test from "node:test";
import assert from "node:assert/strict";
import { newNodeDraft, identityError } from "../src/inventory.js";
test("inventory draft skips configured IDs, gateway ID and occupied slots", () => {
  const view = {
    network: 1,
    gateway: 1,
    inventoryCapacity: 16,
    nodes: [
      { index: 0, network: 1, id: 2 },
      { index: 2, network: 1, id: 3 },
    ],
  };
  const n = newNodeDraft(view);
  assert.equal(n.index, 1);
  assert.equal(n.id, 4);
  assert.equal(n.network, 1);
  assert.equal(n.enabled, false);
  assert.equal(n.hasKey, false);
  view.nodes.push({ index: 1, network: 2, id: 4 });
  assert.equal(newNodeDraft(view).id, 4);
  view.nodes = Array.from({ length: 16 }, (_, i) => ({
    index: i,
    network: 1,
    id: i + 2,
  }));
  assert.equal(newNodeDraft(view), null);
});
test("identity validation keeps radio identities separate and preserves edit identity", () => {
  const nodes = [
    { index: 0, network: 1, id: 2 },
    { index: 1, network: 1, id: 3 },
  ];
  assert.equal(identityError(nodes[0], nodes[0], nodes, 1), "");
  assert.match(
    identityError({ ...nodes[0], id: 3 }, nodes[0], nodes, 1),
    /already registered/,
  );
  assert.equal(
    identityError({ ...nodes[0], id: 3, network: 2 }, nodes[0], nodes, 1),
    "",
  );
  assert.match(
    identityError({ ...nodes[0], id: 1 }, nodes[0], nodes, 1),
    /Gateway ID/,
  );
  for (const id of [0, 65535, 2.5])
    assert.ok(identityError({ ...nodes[0], id }, nodes[0], nodes, 1));
});
