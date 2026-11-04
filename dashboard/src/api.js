export async function api(token, path, body) {
  const r = await fetch(path, {
    method: body ? "POST" : "GET",
    cache: "no-store",
    headers: {
      "X-Commissioning-Token": token,
      ...(body ? { "Content-Type": "application/json" } : {}),
    },
    ...(body ? { body: JSON.stringify(body) } : {}),
  });
  if (!r.ok)
    throw new Error(
      r.status === 401
        ? "Access code rejected. Check Serial Monitor OUTPUT."
        : await r.text(),
    );
  return r.json();
}
export function age(ms) {
  if (ms == null) return "—";
  if (ms < 1000) return "<1s";
  if (ms < 60000) return `${Math.floor(ms / 1000)}s`;
  return `${Math.floor(ms / 60000)}m`;
}
export function utc(t) {
  return t
    ? new Date(t * 1000)
        .toISOString()
        .replace("T", " ")
        .replace(".000Z", " UTC")
    : "UTC unavailable";
}
export function fresh(n) {
  return (
    n.session &&
    n.hasLive &&
    n.live.flags & 1 &&
    n.receiveAge + n.live.sampleAge <= 15000
  );
}
