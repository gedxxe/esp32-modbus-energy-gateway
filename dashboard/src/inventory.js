export const INVENTORY_CAPACITY = 16;

// Local inventory address suggestion, never a remote node-address command.
export function newNodeDraft(view) {
  const capacity = view.inventoryCapacity ?? INVENTORY_CAPACITY;
  const usedSlots = new Set(view.nodes.map((n) => n.index));
  const index = Array.from({ length: capacity }, (_, i) => i).find(
    (i) => !usedSlots.has(i),
  );
  if (index === undefined) return null;
  const usedIds = new Set(
    view.nodes.filter((n) => n.network === view.network).map((n) => n.id),
  );
  let id = 1;
  while (id < 65535 && (id === view.gateway || usedIds.has(id))) id++;
  if (id === 65535) return null;
  return {
    index,
    network: view.network,
    id,
    revision: 0,
    enabled: false,
    hasKey: false,
    name: "",
    location: "",
    notes: "",
  };
}

export function identityError(draft, original, nodes, gateway) {
  if (
    !Number.isInteger(draft.network) ||
    draft.network < 1 ||
    draft.network > 65535
  )
    return "Network ID must be 1–65535.";
  if (!Number.isInteger(draft.id) || draft.id < 1 || draft.id > 65534)
    return "Node ID must be 1–65534.";
  if (draft.id === gateway) return "Node ID must differ from Gateway ID.";
  if (
    nodes.some(
      (n) =>
        n.index !== original.index &&
        n.network === draft.network &&
        n.id === draft.id,
    )
  )
    return "This Network ID / Node ID is already registered.";
  return "";
}
