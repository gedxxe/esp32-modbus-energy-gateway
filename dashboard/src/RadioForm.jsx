import React, { useState } from "react";
import { Field, Badge } from "./ui.jsx";
function RadioForm({ view, t, command, busy }) {
  const [draft, setDraft] = useState({
    network: view.network,
    gateway: view.gateway,
    enabled: view.radioEnabled,
    ...view.phy,
  });
  const update = (k, v) => setDraft({ ...draft, [k]: v });
  return (
    <form
      className="card radio-form"
      onSubmit={(e) => {
        e.preventDefault();
        command({ action: "saveGateway", ...draft });
      }}
    >
      <div className="section-title">
        <div>
          <h2>Gateway configuration</h2>
          <p>One SX1262 uses one shared PHY at a time.</p>
        </div>
        <Badge good={view.initialized}>{view.phase}</Badge>
      </div>
      <div className="form-grid">
        {[
          ["network", "Network ID", 1, 65535],
          ["gateway", "Gateway ID", 1, 65534],
          ["hz", "Frequency, Hz", 803000000, 930000000],
          ["preamble", "Preamble symbols", 8, 64],
          ["sf", "Spreading factor", 7, 12],
          ["cr", "Coding denominator (4/x)", 5, 8],
          ["sync", "Logical sync byte (decimal)", 0, 255],
          ["dbm", "TX power, dBm", 0, 22],
        ].map(([k, label, min, max]) => (
          <Field key={k} label={label}>
            <input
              type="number"
              min={min}
              max={max}
              required
              value={draft[k]}
              onChange={(e) => update(k, Number(e.target.value))}
            />
          </Field>
        ))}
        <Field label="Bandwidth, kHz">
          <select
            value={draft.bw}
            onChange={(e) => update("bw", Number(e.target.value))}
          >
            {[125, 250, 500].map((n) => (
              <option key={n}>{n}</option>
            ))}
          </select>
        </Field>
      </div>
      <div className="note">
        Explicit header · normal IQ · physical LoRa CRC on. Logical sync 18
        (0x12) maps to SX126x register 0x1424. Match the PHY on every node.
        Radio configuration changes are local.
      </div>
      <label className="check">
        <input
          type="checkbox"
          checked={draft.enabled}
          onChange={(e) => update("enabled", e.target.checked)}
        />
        Enable radio after verifying wiring, antenna, PHY, and node keys
      </label>
      <button className="primary" disabled={busy}>
        {t.save}
      </button>
      <p className="footnote">
        Save result appears above. The radio phase and driver code show the
        later apply/initialization result.
      </p>
    </form>
  );
}

export { RadioForm };
