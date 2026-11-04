import React, { useEffect, useRef } from "react";
function Icon({ name = "signal", size = 20 }) {
  const paths = {
    signal: (
      <>
        <path d="M4 20v-3m5 3v-7m5 7V9m5 11V4" />
      </>
    ),
    grid: (
      <>
        <rect x="4" y="4" width="6" height="6" rx="1" />
        <rect x="14" y="4" width="6" height="6" rx="1" />
        <rect x="4" y="14" width="6" height="6" rx="1" />
        <rect x="14" y="14" width="6" height="6" rx="1" />
      </>
    ),
    node: (
      <>
        <circle cx="12" cy="5" r="2" />
        <circle cx="5" cy="18" r="2" />
        <circle cx="19" cy="18" r="2" />
        <path d="m11 7-5 9m7-9 5 9M7 18h10" />
      </>
    ),
    radio: (
      <>
        <circle cx="12" cy="12" r="2" />
        <path d="M8 6a8 8 0 0 0 0 12m8-12a8 8 0 0 1 0 12M5 3a12 12 0 0 0 0 18M19 3a12 12 0 0 1 0 18" />
      </>
    ),
    archive: (
      <>
        <rect x="4" y="7" width="16" height="13" rx="2" />
        <path d="M3 3h18v4H3zm6 9h6" />
      </>
    ),
    diag: (
      <>
        <path d="M3 12h4l3-7 4 14 3-7h4" />
      </>
    ),
  };
  return (
    <svg
      width={size}
      height={size}
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.6"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      {paths[name] || paths.signal}
    </svg>
  );
}
function Field({ label, children, hint }) {
  return (
    <label className="field">
      <span>{label}</span>
      {children}
      {hint && <small>{hint}</small>}
    </label>
  );
}
function Modal({ title, children, onClose }) {
  const ref = useRef(null);
  useEffect(() => {
    ref.current.showModal();
  }, []);
  return (
    <dialog ref={ref} onCancel={onClose}>
      <header>
        <h2>{title}</h2>
        <button className="icon-button" aria-label="Close" onClick={onClose}>
          ×
        </button>
      </header>
      {children}
    </dialog>
  );
}
function Badge({ children, good = false }) {
  return (
    <span className={"badge " + (good ? "good" : "")}>
      <i />
      {children}
    </span>
  );
}
function Stat({ label, value, suffix, detail, warn }) {
  return (
    <section className={"stat " + (warn ? "warn" : "")}>
      <span>{label}</span>
      <div>
        {value}
        <small>{suffix}</small>
      </div>
      <p>{detail}</p>
    </section>
  );
}
function Empty({ text, detail }) {
  return (
    <div className="empty">
      <div className="empty-icon">
        <Icon name="node" size={32} />
      </div>
      <h3>{text}</h3>
      <p>{detail}</p>
    </div>
  );
}
function download(data, name, type) {
  const u = URL.createObjectURL(new Blob([data], { type }));
  const a = document.createElement("a");
  a.href = u;
  a.download = name;
  a.click();
  setTimeout(() => URL.revokeObjectURL(u), 1000);
}

export { Icon, Field, Modal, Badge, Stat, Empty, download };
