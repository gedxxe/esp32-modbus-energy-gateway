import React, { useState } from "react";
import { Field, Modal } from "./ui.jsx";
import { identityError } from "./inventory.js";
function NodeForm({ node, t, onClose, command, busy, nodes, gateway }) {
  const [draft, setDraft] = useState({ ...node, key: "" });
  const update = (k, v) => setDraft((current) => ({ ...current, [k]: v }));
  const conflict = identityError(draft, node, nodes, gateway);
  const changedAddress = draft.network !== node.network || draft.id !== node.id;
  async function save(e) {
    e.preventDefault();
    if (conflict) return;
    if (
      await command({
        action: "saveNode",
        index: node.index,
        node: draft,
        expected: node.expectedRevision,
      })
    ) {
      update("key", "");
      onClose();
    }
  }
  return (
    <Modal
      title={node.revision ? `${t.edit} / Node ${node.id}` : t.add}
      onClose={onClose}
    >
      <form onSubmit={save}>
        <div className="form-grid">
          <Field label="Network ID">
            <input
              type="number"
              min="1"
              max="65535"
              required
              value={draft.network}
              onChange={(e) => update("network", Number(e.target.value))}
            />
          </Field>
          <Field label="Node ID">
            <input
              type="number"
              min="1"
              max="65534"
              required
              value={draft.id}
              onChange={(e) => update("id", Number(e.target.value))}
            />
          </Field>
          <Field label="Name">
            <input
              maxLength="32"
              value={draft.name}
              onChange={(e) => update("name", e.target.value)}
            />
          </Field>
          <Field label="Location">
            <input
              maxLength="48"
              value={draft.location}
              onChange={(e) => update("location", e.target.value)}
            />
          </Field>
        </div>
        <Field
          label="New pairing key · 64 hex digits"
          hint={
            node.hasKey
              ? "Leave blank to retain key for the same address. Stored keys are never returned."
              : "Configure a unique key. Do not use the public test key."
          }
        >
          <input
            type="password"
            autoComplete="off"
            pattern="[a-fA-F0-9]{64}"
            required={draft.enabled && (!node.hasKey || changedAddress)}
            value={draft.key}
            onChange={(e) => update("key", e.target.value)}
          />
        </Field>
        <Field label={t.notes}>
          <textarea
            maxLength="96"
            value={draft.notes}
            onChange={(e) => update("notes", e.target.value)}
          />
        </Field>
        <label className="check">
          <input
            type="checkbox"
            checked={draft.enabled}
            onChange={(e) => update("enabled", e.target.checked)}
          />
          Enabled
        </label>
        <div className="note">
          Node ID, name, settings and key are saved in gateway flash after
          verified save. Live measurements are kept in RAM. Local inventory
          only. Set actual address/key on the node too. Address changes require
          a new key and never relabel old archives. Disable/delete stops
          accepting this node; archives remain.
        </div>
        {conflict && (
          <div role="alert" className="alert error">
            {conflict}
          </div>
        )}
        <div className="form-actions">
          <button className="primary" disabled={busy || Boolean(conflict)}>
            {t.save}
          </button>
          <button type="button" onClick={onClose}>
            {t.cancel}
          </button>
          {node.revision > 0 && (
            <button
              type="button"
              className="danger"
              disabled={busy}
              onClick={async () => {
                if (
                  confirm(
                    "Delete inventory entry? Sessions will clear; historical archives remain.",
                  )
                ) {
                  if (
                    await command({
                      action: "deleteNode",
                      index: node.index,
                      expected: node.expectedRevision,
                    })
                  )
                    onClose();
                }
              }}
            >
              Delete
            </button>
          )}
        </div>
      </form>
    </Modal>
  );
}

export { NodeForm };
