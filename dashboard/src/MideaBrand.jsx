import React from "react";

// Self-contained brandmark: the dashboard must work without an internet link.
// The wordmark is set in the blue holding shape used for Midea identification.
export function MideaBrand({ compact = false }) {
  return (
    <div className={compact ? "midea-brand compact" : "midea-brand"}>
      <div className="midea-brandmark" role="img" aria-label="Midea">
        <svg viewBox="0 0 132 40" xmlns="http://www.w3.org/2000/svg" aria-hidden="true">
          <g fill="none" stroke="#fff" strokeWidth="2.5" strokeLinecap="round">
            <path d="M15 29 C4 19,8 8,22 7 C29 7,34 11,35 15"/>
            <path d="M13 30 C4 16,18 4,31 12"/>
          </g>
          <text x="39" y="27" fill="#fff" fontFamily="Arial,Helvetica,sans-serif"
            fontSize="23" fontWeight="700" letterSpacing="-.9">Midea</text>
        </svg>
      </div>
      <div className="midea-brand-unit">
        <strong>ENGINEERING</strong>
        <span>POWER MONITORING · LoRa GATEWAY</span>
      </div>
    </div>
  );
}
