import React from "react";
import { Icon, Field } from "./ui.jsx";
import { age, fresh, utc } from "./api.js";

const words = {
  en: {
    title: "Node activity",
    saved: "Inventory saved in gateway flash",
    add: "Add node",
    addHint: "Register an ID, name and pairing key",
    full: "Inventory full",
    noFlash: "Flash unavailable",
    fresh: "Fresh data",
    stale: "Stale data",
    missing: "No measurement",
    disabled: "Disabled",
    pending: "Awaiting HELLO",
    candidate: "Confirming session",
    link: "Link health",
    confirmed: "Session confirmed",
    unconfirmed: "Session unconfirmed",
    heard: "Last authenticated packet",
    never: "No accepted packets yet",
    packet: "Latest frame",
    accepted: "New packets",
    duplicate: "Duplicate RX",
    received: "Received data",
    live: "Request live",
    checkpoint: "Request checkpoint",
    edit: "Edit node",
    revision: "Saved revision",
    liveNote:
      "Live readings are in RAM. Checkpoint archives are stored separately in flash.",
    source: "Source UTC",
    sample: "Sample age at RX",
    receive: "Telemetry received",
    fw: "Node firmware",
    stored: "Key configured",
    notStored: "Key not configured",
    sampleStale: "Last valid kWh reading. Check the timestamp before using it as a current value.",
    now: "Last valid energy reading",
    transaction: "Request",
    capture: "Checkpoint request",
    day: "WIB day number",
    dayHint: "0 asks for the node’s active capture day.",
    request: "Request",
    cancel: "Cancel",
  },
  id: {
    title: "Aktivitas node",
    saved: "Inventori disimpan di flash gateway",
    add: "Tambah node",
    addHint: "Daftarkan ID, nama, dan pairing key",
    full: "Inventori penuh",
    noFlash: "Flash tidak tersedia",
    fresh: "Data fresh",
    stale: "Data stale",
    missing: "Belum ada pengukuran",
    disabled: "Nonaktif",
    pending: "Menunggu HELLO",
    candidate: "Konfirmasi sesi",
    link: "Kesehatan link",
    confirmed: "Sesi terkonfirmasi",
    unconfirmed: "Sesi belum terkonfirmasi",
    heard: "Paket terautentikasi terakhir",
    never: "Belum ada paket diterima",
    packet: "Frame terakhir",
    accepted: "Paket baru",
    duplicate: "RX duplikat",
    received: "Data diterima",
    live: "Minta live",
    checkpoint: "Minta checkpoint",
    edit: "Ubah node",
    revision: "Revisi tersimpan",
    liveNote:
      "Pembacaan live ada di RAM. Arsip checkpoint disimpan terpisah di flash.",
    source: "UTC sumber",
    sample: "Umur sampel saat RX",
    receive: "Telemetry diterima",
    fw: "Firmware node",
    stored: "Key terkonfigurasi",
    notStored: "Key belum diisi",
    sampleStale: "Pembacaan kWh valid terakhir. Periksa waktunya sebelum dianggap sebagai nilai saat ini.",
    now: "Pembacaan energi valid terakhir",
    transaction: "Permintaan",
    capture: "Permintaan checkpoint",
    day: "Nomor hari WIB",
    dayHint: "0 meminta capture day aktif pada node.",
    request: "Minta",
    cancel: "Batal",
  },
};
const frameNames = {
  1: "HELLO",
  2: "ACK",
  3: "TELEMETRY",
  4: "POLL",
  5: "CHECKPOINT",
};
const requestNames = {
  idle: "Idle",
  reserving: "Preparing",
  queued: "Queued",
  waiting: "Waiting for node",
  validated: "Validated",
  durable: "Saved to flash",
  timeout_unavailable: "Timeout / unavailable",
  error: "Failed",
};
function energy(value) {
  return Number.isFinite(value)
    ? value.toLocaleString(undefined, { maximumFractionDigits: 3 })
    : "—";
}
function DataItem({ label, children }) {
  return (
    <div>
      <dt>{label}</dt>
      <dd>{children}</dd>
    </div>
  );
}
function ReceivedData({ node: n, w }) {
  const l = n.live;
  return (
    <details className="received-data">
      <summary>
        {w.received}
        <span aria-hidden="true">⌄</span>
      </summary>
      <dl className="received-fields">
        <DataItem label={w.packet}>
          {frameNames[n.lastType] || "—"} / #{n.lastSequence || "—"}
        </DataItem>
        <DataItem label="Network / node">
          {n.network} / {n.id}
        </DataItem>
        <DataItem label="Session">{n.session || n.candidate || "—"}</DataItem>
        <DataItem label={w.fw}>{n.firmware || "—"}</DataItem>
        {n.hasLive && (
          <>
            <DataItem label="Last received kWh">
              {l.flags & 1 ? energy(l.kwh) : "—"}
            </DataItem>
            <DataItem label={w.source}>{utc(l.flags & 2 ? l.utc : 0)}</DataItem>
            <DataItem label="Sample age / receive age">
              {age(l.sampleAge)} / {age(n.receiveAge)}
            </DataItem>
            <DataItem label="Profile / saved meter">
              {l.profile} / {l.meterId}
            </DataItem>
            <DataItem label="WIB day / checkpoint revision">
              {l.day || "—"} / {l.revision}
            </DataItem>
            <DataItem label="Validity flags">
              0x{l.flags.toString(16).padStart(2, "0")}
            </DataItem>
            <DataItem label="Request correlation">
              {l.correlation ?? 0}
            </DataItem>
          </>
        )}
      </dl>
      {n.hasLive && !fresh(n) && <p>{w.sampleStale}</p>}
      <p>{w.liveNote}</p>
    </details>
  );
}
function NodeCard({ node: n, w, edit, command, busy, requestCheckpoint }) {
  const isFresh = Boolean(n.enabled && fresh(n));
  const latestReading = n.hasLastValid && n.lastValid ? n.lastValid : n.live;
  const hasReading = Boolean((n.hasLastValid || (n.hasLive && (n.live.flags & 1))) && Number.isFinite(latestReading?.kwh));
  const readingAge = n.hasLastValid ? n.lastValidAge : n.receiveAge;
  const requestPending = ["reserving", "queued", "waiting", "validated"].includes(n.request);
  const status = !n.enabled
    ? w.disabled
    : isFresh
      ? w.fresh
      : !n.session
        ? n.candidate
          ? w.candidate
          : w.pending
        : n.hasLive
          ? w.stale
          : w.missing;
  const hasPacket = n.packetAge !== null && n.packetAge !== undefined;
  return (
    <article
      className={`node-card ${isFresh ? "is-fresh" : n.candidate ? "is-candidate" : ""} ${!n.enabled ? "is-disabled" : ""}`}
      aria-label={`Node ${n.id}, network ${n.network}`}
    >
      <header className="node-card-heading">
        <div>
          <span className="eyebrow">NETWORK {n.network}</span>
          <h2>Node {String(n.id).padStart(2, "0")}</h2>
        </div>
        <span className={`node-status ${isFresh ? "fresh" : ""}`}>
          <i />
          {status}
        </span>
      </header>
      <div className="node-card-name">
        <h3>{n.name || `Node ${n.id}`}</h3>
        <p>{n.location || "—"}</p>
      </div>
      <div className="node-energy">
        <span>{w.now}</span>
        <b className="energy">
          {hasReading ? energy(latestReading.kwh) : "—"} <small>kWh</small>
        </b>
        <small>
          {w.receive}: {hasReading ? age(readingAge) : "—"} · {w.sample}:{" "}
          {n.hasLive ? age(n.live.sampleAge) : "—"}
        </small>
        {hasReading && !isFresh && <p className="reading-note">{w.sampleStale}</p>}
      </div>
      <section className="link-health" aria-label={w.link}>
        <div className="link-title">
          <Icon name="signal" size={15} />
          <b>{w.link}</b>
          <span>{n.session ? w.confirmed : w.unconfirmed}</span>
        </div>
        <div className="link-values">
          <span>
            RSSI <strong>{hasPacket ? `${n.rssi} dBm` : "—"}</strong>
          </span>
          <span>
            SNR <strong>{hasPacket ? `${n.snr} dB` : "—"}</strong>
          </span>
        </div>
        <p>{hasPacket ? `${w.heard}: ${age(n.packetAge)}` : w.never}</p>
        <div className="packet-counts">
          <span>
            {w.accepted} <b>{n.accepted ?? 0}</b>
          </span>
          <span>
            {w.duplicate} <b>{n.duplicates ?? 0}</b>
          </span>
        </div>
      </section>
      <div className="node-transaction">
        <span>{w.transaction}</span>
        <strong>{requestNames[n.request] || n.request}</strong>
        <small>#{n.correlation || "—"}</small>
      </div>
      <ReceivedData node={n} w={w} />
      <div className="node-card-actions">
        <button
          disabled={busy || requestPending || !n.enabled || !n.session}
          onClick={() => command({ action: "live", index: n.index })}
        >
          {w.live}
        </button>
        <button
          disabled={busy || !n.enabled || !n.session}
          onClick={() => requestCheckpoint(n)}
        >
          {w.checkpoint}
        </button>
        <button
          className="node-edit"
          onClick={() => edit(n)}
          aria-label={`${w.edit} ${n.id}`}
        >
          ↗
        </button>
      </div>
      <div className="node-card-footer">
        <span>{n.hasKey ? w.stored : w.notStored}</span>
        <span>
          {w.revision} {n.revision}
        </span>
      </div>
    </article>
  );
}
function NodeGrid({ view, lang, edit, add, command, busy, requestCheckpoint }) {
  const w = words[lang] || words.en;
  const capacity = view.inventoryCapacity ?? 16;
  const full = view.nodes.length >= capacity;
  return (
    <section className="node-grid-section" aria-labelledby="node-grid-title">
      <div className="section-title">
        <div>
          <h2 id="node-grid-title">
            {w.title}{" "}
            <span className="count">
              {view.nodes.length} / {capacity}
            </span>
          </h2>
          <p>
            <span className={`flash-dot ${view.storage ? "ready" : ""}`} />
            {view.storage ? w.saved : w.noFlash} · {w.revision} {view.revision}
          </p>
        </div>
      </div>
      <div className="node-grid">
        {view.nodes.map((n) => (
          <NodeCard
            key={`${n.network}:${n.id}`}
            node={n}
            w={w}
            edit={edit}
            command={command}
            busy={busy}
            requestCheckpoint={requestCheckpoint}
          />
        ))}
        <button
          className="add-node-card"
          onClick={add}
          disabled={full || !view.storage || busy}
        >
          <span className="add-node-plus" aria-hidden="true">
            +
          </span>
          <strong>{full ? w.full : w.add}</strong>
          <span>
            {full
              ? `${capacity} / ${capacity}`
              : !view.storage
                ? w.noFlash
                : w.addHint}
          </span>
        </button>
      </div>
    </section>
  );
}
function CheckpointForm({ node, w, onClose, command, busy }) {
  const [day, setDay] = React.useState("0");
  return (
    <form
      onSubmit={async (e) => {
        e.preventDefault();
        if (
          await command({
            action: "checkpoint",
            index: node.index,
            day: Number(day),
            expected: node.expectedRevision,
          })
        )
          onClose();
      }}
    >
      <Field label={w.day} hint={w.dayHint}>
        <input
          type="number"
          min="0"
          max="47481"
          step="1"
          required
          value={day}
          onChange={(e) => setDay(e.target.value)}
        />
      </Field>
      <div className="form-actions">
        <button className="primary" disabled={busy}>
          {w.request}
        </button>
        <button type="button" onClick={onClose}>
          {w.cancel}
        </button>
      </div>
    </form>
  );
}
export { NodeGrid, CheckpointForm, words as nodeWords };
