// Local QA server for the actual production bundle. RF/telemetry fixtures are labelled simulated.
import http from "node:http";
import { readFile } from "node:fs/promises";
import { cardFixture } from "./test/fixture.mjs";
let state = cardFixture(),
  id = 0;
const server = http.createServer(async (req, res) => {
  if (req.url === "/") {
    res.setHeader("Content-Type", "text/html");
    res.end(await readFile("dist/index.html"));
    return;
  }
  if (req.url === "/api/status") {
    res.setHeader("Content-Type", "application/json");
    res.end(JSON.stringify(state));
    return;
  }
  if (req.url === "/api/archives") {
    res.setHeader("Content-Type", "application/json");
    res.end(JSON.stringify({ archives: [] }));
    return;
  }
  if (req.url === "/api/logs") {
    res.end("SIMULATED QA — no RF hardware connected\n");
    return;
  }
  if (req.url === "/api/command") {
    let body = "";
    for await (const x of req) body += x;
    if (body.length > 1536) {
      res.writeHead(413);
      res.end();
      return;
    }
    const c = JSON.parse(body);
    const r = {
      id: ++id,
      ok: true,
      message: "SIMULATED QA: command accepted (no device writes)",
    };
    state.results = [r];
    if (c.action === "saveNode") {
      const n = {
        ...state.nodes[0],
        ...c.node,
        index: c.index,
        hasKey: true,
        revision: 1,
        session: null,
        candidate: null,
        hasLive: false,
        packetAge: null,
        accepted: 0,
        duplicates: 0,
        lastType: 0,
        lastSequence: 0,
        firmware: "",
        request: "idle",
        live: {
          flags: 0,
          utc: 0,
          sampleAge: 0,
          profile: 0,
          meterId: 0,
          day: 0,
          revision: 0,
          correlation: 0,
          kwh: null,
        },
      };
      delete n.key;
      state.nodes = state.nodes.filter((n) => n.index !== c.index).concat(n);
      state.revision++;
    }
    if (c.action === "deleteNode") {
      state.nodes = state.nodes.filter((n) => n.index !== c.index);
      state.revision++;
    }
    res.setHeader("Content-Type", "application/json");
    res.end(JSON.stringify({ id }));
    return;
  }
  res.writeHead(404);
  res.end();
});
server.listen(4173, "127.0.0.1", () =>
  console.log(
    "Local: http://127.0.0.1:4173 — SIMULATED QA, actual production assets",
  ),
);
