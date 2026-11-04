import React, { useEffect, useRef, useState } from "react";
import { createRoot } from "react-dom/client";
import { api, age, fresh } from "./api.js";
import { Icon, Field, Badge, Stat, Empty, Modal, download } from "./ui.jsx";
import { NodeForm } from "./Nodes.jsx";
import { NodeGrid, CheckpointForm, nodeWords } from "./NodeCards.jsx";
import { newNodeDraft } from "./inventory.js";
import { RadioForm } from "./RadioForm.jsx";
import { MideaBrand } from "./MideaBrand.jsx";

const copy = {
  en: {
    overview: "Overview",
    inventory: "Node inventory",
    radio: "Gateway & radio",
    diagnostics: "Diagnostics",
    archives: "Checkpoints",
    commissioning: "LOCAL COMMISSIONING",
    subtitle: "Monitor the latest valid power-meter readings, inspect radio connectivity and request a new sample from an individual node when needed. Telemetry arrives over authenticated LoRa; the displayed value is not always a real-time reading.",
    add: "Add node",
    live: "Request live",
    checkpoint: "Request checkpoint",
    save: "Save & apply",
    cancel: "Cancel",
    empty: "Your network starts here.",
    emptyText:
      "Register a node with its own pairing key, match its radio settings, then enable the gateway.",
    connect: "Open gateway",
    code: "Commissioning access code",
    intro:
      "Connect your device to the gateway access point, then enter the commissioning code printed on the Serial Monitor at 115200 baud. This local dashboard does not require internet access.",
    disabled: "Radio disabled",
    nodes: "Registered nodes",
    confirmed: "Confirmed sessions",
    budget: "Gateway TX load",
    storage: "Durable archives",
    settings: "Settings",
    logout: "Lock",
    edit: "Edit",
    notes: "Commissioning notes",
  },
  id: {
    overview: "Ringkasan",
    inventory: "Inventori node",
    radio: "Gateway & radio",
    diagnostics: "Diagnostik",
    archives: "Checkpoint",
    commissioning: "COMMISSIONING LOKAL",
    subtitle: "Pantau pembacaan power meter terakhir, kondisi komunikasi radio, dan minta sampel baru dari masing-masing node ketika diperlukan. Data dikirim melalui LoRa yang terautentikasi; angka terakhir tidak selalu merupakan hasil ukur real-time.",
    add: "Tambah node",
    live: "Minta live",
    checkpoint: "Minta checkpoint",
    save: "Simpan & terapkan",
    cancel: "Batal",
    empty: "Mulai jaringan Anda di sini.",
    emptyText:
      "Daftarkan node dengan pairing key tersendiri, cocokkan radio, lalu aktifkan gateway.",
    connect: "Buka gateway",
    code: "Kode akses commissioning",
    intro:
      "Hubungkan perangkat ke access point gateway, kemudian masukkan kode commissioning yang tampil di Serial Monitor pada 115200 baud. Dashboard lokal ini tidak membutuhkan akses internet.",
    disabled: "Radio nonaktif",
    nodes: "Node terdaftar",
    confirmed: "Sesi terkonfirmasi",
    budget: "Beban TX gateway",
    storage: "Arsip durable",
    settings: "Pengaturan",
    logout: "Kunci",
    edit: "Ubah",
    notes: "Catatan commissioning",
  },
};
function App() {
  const [lang, setLang] = useState("en"),
    [token, setToken] = useState(""),
    [code, setCode] = useState(""),
    [view, setView] = useState(null),
    [receivedAt, setReceivedAt] = useState(0),
    [clock, setClock] = useState(0),
    [page, setPage] = useState("overview"),
    [error, setError] = useState(""),
    [notice, setNotice] = useState(""),
    [editing, setEditing] = useState(null),
    [checkpointNode, setCheckpointNode] = useState(null),
    [busy, setBusy] = useState(false),
    [archives, setArchives] = useState([]),
    [interval, setIntervalValue] = useState(3600);
  const t = copy[lang],
    pending = useRef(new Set());
  function addNode() {
    const draft = newNodeDraft(view);
    if (draft) setEditing({ ...draft, expectedRevision: view.revision });
    else setError("Inventory capacity reached.");
  }
  function editNode(node) {
    setEditing({ ...node, expectedRevision: view.revision });
  }
  function requestCheckpoint(node) {
    setCheckpointNode({ ...node, expectedRevision: view.revision });
  }
  async function refresh(key = token) {
    const v = await api(key, "/api/status");
    const now = performance.now();
    setReceivedAt(now);
    setClock(now);
    setView(v);
    for (const r of v.results || [])
      if (pending.current.has(r.id)) {
        pending.current.delete(r.id);
        setNotice(r.message);
        if (!r.ok) setError(r.message);
      }
  }
  useEffect(() => {
    if (!token) return;
    refresh().catch((e) => setError(e.message));
    const id = setInterval(
      () => refresh().catch((e) => setError(e.message)),
      3000,
    );
    return () => clearInterval(id);
  }, [token]);
  useEffect(() => {
    if (!token) return;
    const id = setInterval(() => setClock(performance.now()), 1000);
    return () => clearInterval(id);
  }, [token]);
  async function command(body) {
    setBusy(true);
    setError("");
    setNotice("");
    try {
      const r = await api(token, "/api/command", {
        ...body,
        expected: body.expected ?? view.revision,
      });
      pending.current.add(r.id);
      setNotice(`Command #${r.id} queued. Waiting for save/apply result…`);
      return true;
    } catch (e) {
      setError(e.message);
      return false;
    } finally {
      setBusy(false);
    }
  }
  async function login(e) {
    e.preventDefault();
    setError("");
    try {
      await refresh(code);
      setToken(code);
      setCode("");
    } catch (e) {
      setError(e.message);
    }
  }
  async function exportLogs() {
    try {
      const r = await fetch("/api/logs", {
        headers: { "X-Commissioning-Token": token },
      });
      if (!r.ok) throw Error(await r.text());
      download(await r.text(), "gateway-log.txt", "text/plain");
    } catch (e) {
      setError(e.message);
    }
  }
  async function loadArchives() {
    try {
      const r = await api(token, "/api/archives");
      setArchives(r.archives);
    } catch (e) {
      setError(e.message);
    }
  }
  useEffect(() => {
    if (page === "archives" && token) loadArchives();
  }, [page, token, view?.archiveCount]);
  if (!token)
    return (
      <main className="login">
        <div className="login-brand"><MideaBrand /></div>
        <form onSubmit={login}>
          <span className="eyebrow">{t.commissioning} · ESP32-C3</span>
          <h1>{lang === "id" ? "Monitoring daya melalui LoRa" : "LoRa power monitoring"}</h1>
          <p>{t.intro}</p>
          <Field label={t.code}>
            <input
              type="password"
              autoComplete="off"
              value={code}
              onChange={(e) => setCode(e.target.value)}
              required
              maxLength={32}
            />
          </Field>
          {error && (
            <div role="alert" className="alert error">
              {error}
            </div>
          )}
          <button className="primary">
            {t.connect} <span>↗</span>
          </button>
          <small>ESP32-C3 · SX1262 · Energy Station protocol v1</small>
        </form>
        <div className="login-footer">
          <span>Midea Engineering · Local commissioning</span>
          <button onClick={() => setLang(lang === "en" ? "id" : "en")}>
            {lang === "en" ? "Bahasa Indonesia" : "English"}
          </button>
        </div>
      </main>
    );
  if (!view) return <main>Loading gateway…</main>;
  const elapsed = Math.max(0, clock - receivedAt);
  const nodes = view.nodes.map((n) => ({
      ...n,
      receiveAge: n.hasLive ? n.receiveAge + elapsed : n.receiveAge,
      lastValidAge: n.hasLastValid ? n.lastValidAge + elapsed : n.lastValidAge,
      packetAge: n.packetAge == null ? null : n.packetAge + elapsed,
    })),
    enabled = nodes.filter((n) => n.enabled),
    confirmed = nodes.filter((n) => n.session),
    ackLoad = ((enabled.length * view.ackAirMs) / (interval * 1000)) * 100;
  const tabs = [
    ["overview", "grid"],
    ["inventory", "node"],
    ["radio", "radio"],
    ["archives", "archive"],
    ["diagnostics", "diag"],
  ];
  return (
    <div className="shell">
      <aside>
        <div className="brand"><MideaBrand compact /></div>
        <div className="side-label">POWER MONITORING</div>
        <nav aria-label="Main navigation">
          {tabs.map(([key, icon]) => (
            <button
              key={key}
              className={page === key ? "active" : ""}
              onClick={() => setPage(key)}
            >
              <Icon name={icon} />
              {t[key]}
              {page === key && <b>•</b>}
            </button>
          ))}
        </nav>
        <div className="side-bottom">
          <div className="device-dot" />
          <b>ESP32-C3</b>
          <small>
            LoRa gateway · Protocol v1
            <br />
            Local AP · 192.168.4.1
          </small>
          <button
            onClick={() => {
              setToken("");
              setView(null);
            }}
          >
            {t.logout} ↗
          </button>
        </div>
      </aside>
      <div className="workspace">
        <header className="topbar">
          <span>
            <span className="muted">Midea Engineering</span> / {t[page]}
          </span>
          <div>
            <span className="local-chip">
              <i />
              Gateway AP
            </span>
            <button onClick={() => setLang(lang === "en" ? "id" : "en")}>
              {lang === "en" ? "EN / ID" : "ID / EN"}
            </button>
          </div>
        </header>
        <main className="content">
          {view.simulated && (
            <div className="simulation-banner">
              SIMULATED UI PREVIEW · No RF hardware connected
            </div>
          )}
          <div className="page-heading">
            <div>
              <span className="eyebrow">
                {t.commissioning} / NETWORK {view.network}
              </span>
              <h1>
                {t[page]}

              </h1>
              <p>{t.subtitle}</p>
            </div>
            <Badge good={view.initialized}>
              {view.radioEnabled ? view.phase : t.disabled}
            </Badge>
          </div>
          {error && (
            <div className="alert error" role="alert">
              {error}
              <button onClick={() => setError("")}>×</button>
            </div>
          )}
          {notice && (
            <div className="alert" role="status">
              {notice}
            </div>
          )}
          {page === "overview" && (
            <>
              <div className="stats">
                <Stat
                  label={t.nodes}
                  value={nodes.length.toString().padStart(2, "0")}
                  suffix="/ 16"
                  detail="Inventory capacity"
                />
                <Stat
                  label={t.confirmed}
                  value={confirmed.length.toString().padStart(2, "0")}
                  detail={`${nodes.filter(fresh).length} fresh measurements`}
                />
                <Stat
                  label={t.budget}
                  value={ackLoad.toFixed(2)}
                  suffix="%"
                  detail="Estimated periodic ACKs only"
                  warn={ackLoad > 1}
                />
                <Stat
                  label={t.storage}
                  value={view.archiveCount.toString().padStart(2, "0")}
                  suffix="/ 64"
                  detail={
                    view.archiveHealthy
                      ? "Local filesystem healthy"
                      : "Archive storage unavailable"
                  }
                />
              </div>
              <div className="active-phy-strip">
                <span>
                  <Icon name="radio" size={16} />{" "}
                  {(view.phy.hz / 1e6).toFixed(3)} MHz · SF{view.phy.sf} · BW{" "}
                  {view.phy.bw} kHz · CR 4/{view.phy.cr}
                </span>
                <button onClick={() => setPage("radio")}>
                  Shared radio settings ↗
                </button>
              </div>
              <NodeGrid
                view={{ ...view, nodes }}
                lang={lang}
                edit={editNode}
                add={addNode}
                command={command}
                busy={busy}
                requestCheckpoint={requestCheckpoint}
              />
              <div className="footnote">
                ACK confirms packet acceptance. Validated records and durable
                storage are reported separately. UTC is unavailable on this
                gateway.
              </div>
            </>
          )}
          {page === "inventory" && (
            <>
              <NodeGrid
                view={{ ...view, nodes }}
                lang={lang}
                edit={editNode}
                add={addNode}
                command={command}
                busy={busy}
                requestCheckpoint={requestCheckpoint}
              />
              <div className="note">
                Inventory changes apply locally and clear sessions for
                reconfirmation. Change the actual node address on the node
                itself. Old archives retain their original source identity.
                Modbus slave ID, saved meter ID, and profile ID are separate
                metadata.
              </div>
            </>
          )}
          {page === "radio" && (
            <RadioForm view={view} t={t} command={command} busy={busy} />
          )}
          {page === "archives" && (
            <section className="card">
              <div className="section-title">
                <div>
                  <h2>Durable checkpoint records</h2>
                  <p>Schema 3 · ten immutable WIB capture points per record</p>
                </div>
                <button onClick={loadArchives}>Refresh ↻</button>
              </div>
              {!archives.length ? (
                <Empty
                  text="No durable checkpoints yet."
                  detail="Confirm a node session, then request a day. Missing days time out as unavailable."
                />
              ) : (
                <div className="table-scroll">
                  <table>
                    <thead>
                      <tr>
                        <th>Source</th>
                        <th>WIB day</th>
                        <th>Revision</th>
                        <th>Gateway receive reference</th>
                        <th>Export</th>
                      </tr>
                    </thead>
                    <tbody>
                      {archives.map((a) => (
                        <tr key={a.index}>
                          <td>
                            NET {a.network} / NODE {a.node}
                          </td>
                          <td>{a.day}</td>
                          <td>{a.revision}</td>
                          <td>
                            Boot {a.boot} · {age(a.received)} monotonic
                          </td>
                        </tr>
                      ))}
                    </tbody>
                  </table>
                </div>
              )}
              <p className="footnote">
                Records are append-only with 64 slots. A full or unavailable
                store reports an error. Existing records stay intact; no
                automatic formatting or eviction.
              </p>
            </section>
          )}
          {page === "diagnostics" && (
            <>
              <div className="diagnostic-grid">
                <section className="card">
                  <span className="eyebrow">01 / INITIALIZATION & SPI</span>
                  <h2>{view.initialized ? "Radio initialized" : view.phase}</h2>
                  <dl>
                    <dt>Driver code</dt>
                    <dd>{view.radioCode}</dd>
                    <dt>Driver failures</dt>
                    <dd>{view.driverErrors}</dd>
                    <dt>Receive mode</dt>
                    <dd>{view.phase}</dd>
                  </dl>
                  <p>
                    Initialization success proves SPI/driver setup. Empty RX
                    alone does not prove a failed module.
                  </p>
                </section>
                <section className="card">
                  <span className="eyebrow">02 / RF OBSERVATION</span>
                  <h2>{view.counters.rx} packets observed</h2>
                  <dl>
                    <dt>TX / ACK</dt>
                    <dd>
                      {view.counters.tx} / {view.counters.acks}
                    </dd>
                    <dt>Physical CRC failures</dt>
                    <dd>{view.physicalCRC}</dd>
                    <dt>Airtime gate remaining</dt>
                    <dd>{age(view.dutyWait)}</dd>
                  </dl>
                  <p>
                    One active PHY. Uplinks contend for airtime and can collide.
                  </p>
                </section>
                <section className="card">
                  <span className="eyebrow">03 / PROTOCOL VALIDATION</span>
                  <h2>{view.counters.accepted} packets accepted</h2>
                  <dl>
                    {[
                      "auth",
                      "malformed",
                      "unknown",
                      "disabled",
                      "replay",
                      "session",
                      "correlation",
                      "duplicates",
                      "lateAck",
                    ].map((k) => (
                      <React.Fragment key={k}>
                        <dt>{k}</dt>
                        <dd>{view.counters[k]}</dd>
                      </React.Fragment>
                    ))}
                  </dl>
                </section>
                <section className="card">
                  <span className="eyebrow">04 / TRANSACTIONS & STORAGE</span>
                  <h2>{view.counters.durable} durable this boot</h2>
                  <dl>
                    {[
                      "validated",
                      "recordErrors",
                      "timeouts",
                      "storageErrors",
                    ].map((k) => (
                      <React.Fragment key={k}>
                        <dt>{k}</dt>
                        <dd>{view.counters[k]}</dd>
                      </React.Fragment>
                    ))}
                    <dt>Queue / drops / expired</dt>
                    <dd>
                      {view.txDepth} / {view.queueDrops} / {view.expired}
                    </dd>
                    <dt>Config / sequences / archive FS</dt>
                    <dd>
                      {view.storage ? "OK" : "ERROR"} /{" "}
                      {view.sequences ? "OK" : "UNSAFE"} /{" "}
                      {view.archiveHealthy ? "OK" : "ERROR"}
                    </dd>
                    <dt>Free / minimum heap</dt>
                    <dd>
                      {view.freeHeap || "—"} / {view.minimumHeap || "—"} B
                    </dd>
                    <dt>Stack min: radio / storage / HTTP</dt>
                    <dd>
                      {view.radioStackMin || "—"} /{" "}
                      {view.storageStackMin || "—"} / {view.httpStackMin || "—"}{" "}
                      B
                    </dd>
                    <dt>Sequence slots</dt>
                    <dd>{view.sessionSlots} / 64</dd>
                    <dt>Local flash used</dt>
                    <dd>
                      {view.flashUsed} / {view.flashTotal} B
                    </dd>
                  </dl>
                </section>
              </div>
              <section className="card capacity">
                <h2>LoRa airtime and refresh policy</h2>
                <Field label="Node transmission interval for estimation (seconds)">
                  <input
                    type="number"
                    min="30"
                    max="3600"
                    value={interval}
                    onChange={(e) =>
                      setIntervalValue(
                        Math.max(30, Number(e.target.value) || 60),
                      )
                    }
                  />
                </Field>
                <p>
                  ACK airtime {view.ackAirMs.toFixed(2)} ms · telemetry airtime{" "}
                  {view.telemetryAirMs.toFixed(2)} ms · estimated{" "}
                  {ackLoad.toFixed(2)}% gateway TX for {enabled.length} enabled
                  nodes.
                </p>
                <div className="budget-track">
                  <i
                    style={{ width: `${Math.min(100, (ackLoad / 1) * 100)}%` }}
                  />
                </div>
                <p>
                  {ackLoad > 0.7
                    ? "Increase node intervals and leave room for HELLO, retries, POLLs, and fragments."
                    : "Leave headroom for HELLO, retries, POLLs, and checkpoint ACKs."}{" "}
                  This is a calculation, not measured RF capacity. Node
                  intervals are changed on each node. The gateway requests fresh telemetry only when a node has missed its expected update; automatic requests are rate-limited and do not reserve TDMA slots.
                </p>
                <p>Automatic live requests: {view.automaticPolls ?? 0} · Queue/storage errors: {view.automaticPollErrors ?? 0}. Requests start after a one-hour interval plus a two-minute grace period and are spaced apart across nodes.</p>
                <button onClick={exportLogs}>Export bounded log ↓</button>
              </section>
            </>
          )}
          {editing && (
            <NodeForm
              node={editing}
              t={t}
              onClose={() => setEditing(null)}
              command={command}
              busy={busy}
              nodes={nodes}
              gateway={view.gateway}
            />
          )}
          {checkpointNode && (
            <Modal
              title={`${(nodeWords[lang] || nodeWords.en).capture} / Node ${checkpointNode.id}`}
              onClose={() => setCheckpointNode(null)}
            >
              <CheckpointForm
                node={checkpointNode}
                w={nodeWords[lang] || nodeWords.en}
                onClose={() => setCheckpointNode(null)}
                command={command}
                busy={busy}
              />
            </Modal>
          )}
          <footer>
            <span>MIDEA ENGINEERING / LORA GATEWAY</span>
            <span>
              Boot {view.boot} · {age(view.uptime)} uptime
            </span>
          </footer>
        </main>
      </div>
    </div>
  );
}
createRoot(document.getElementById("root")).render(<App />);
