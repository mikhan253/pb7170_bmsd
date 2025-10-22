// ---------- Helpers ----------
const safeArr = v => Array.isArray(v) ? v : [];
const avg = arr => arr?.length ? arr.reduce((a, b) => a + b, 0) / arr.length : 0;
const sum = arr => arr?.length ? arr.reduce((a, b) => a + b, 0) : 0;

const progressBar = (value = 0, type = "primary") => {
    const pct = Math.min(100, Math.max(0, Number(value) || 0));
    return `
        <div class="progress" style="height:20px">
            <div class="progress-bar bg-${type}" role="progressbar" style="width:${pct}%"
                aria-valuenow="${pct}" aria-valuemin="0" aria-valuemax="100">
                ${pct.toFixed(1)}%
            </div>
        </div>`;
};

const decodeBits = (value = 0, texts = [], badgeClass) => {
    const bits = texts
        .map((text, i) => value & (1 << i) ? `<span class="${badgeClass} me-1">${text}</span>` : "")
        .filter(Boolean);
    return bits.length ? bits.join(" ") : `<span class="text-muted">Keine</span>`;
};

const getHeaderClass = (param) =>
    param=="DISABLED" ? "bg-dark text-white" :
    param=="ERROR" ? "bg-danger text-white" :
    param=="RUN_WARN" ? "bg-warning text-dark" :
    param=="RUN" ? "bg-primary text-white" :
    "bg-secondary text-dark";

const findCellLocation = (packs, targetVal) => {
    for (const { name, cells } of packs) {
        const idx = safeArr(cells).indexOf(targetVal);
        if (idx >= 0) return `${name} (C${idx + 1})`;
    }
    return "—";
};

const renderMOSGlobal = (index, packs) => {
    const onStates = packs.map(p => !!(p.mosfetStatus & (1 << index)));
    const allOn = onStates.every(Boolean);
    const anyOn = onStates.some(Boolean);
    const cls = allOn ? "badge-mos-on" : anyOn ? "badge-mos-part" : "badge-mos-off";
    return `<span class="${cls} me-2">${MOS_BITS[index]}</span>`;
};

const renderPackBadges = (list, cls) =>
    list?.length ? list.map(n => `<span class="${cls} me-1">${n}</span>`).join("") :
    "<span class='text-muted'>Keine</span>";

// ---------- Data Fetch ----------
async function fetchData() {
    try {
        const res = await fetch("/api/bmsdata", { cache: "no-store" });
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        const data = CBOR.decode(await res.arrayBuffer());
        renderPacks(data);
    } catch (e) {
        console.error("API Fehler:", e);
        document.getElementById("packsContainer").innerHTML =
            `<div class="col-12"><div class="alert alert-danger">Fehler beim Laden: ${e.message}</div></div>`;
    }
}

// ---------- Main Render ----------
function renderPacks(data) {
    const container = document.getElementById("packsContainer");
    const packs = safeArr(data?.pack)
        .filter(p => p?.id)
        .map(p => ({ ...p, name: `Pack ${p.id} - ${PACKNAMES_TEXT[p.id-1]}` }));

    if (!packs.length) {
        container.innerHTML = `<div class="col-12"><div class="alert alert-secondary">Keine gültigen Packs</div></div>`;
        return;
    }

    const allCells = packs.flatMap(p => safeArr(p.cells).filter(Number.isFinite));
    const allTemps = packs.flatMap(p => safeArr(p.ntcTemperature).filter(Number.isFinite));
    const [maxCellVal, minCellVal] = [Math.max(...allCells), Math.min(...allCells)];

    const packsWithError = packs.filter(p => p.swAlertFlags).map(p => p.name);
    const packsWithWarn = packs.filter(p => p.swWarningFlags).map(p => p.name);

    const global = data.glob ?? {};
    const mosStatus = MOS_BITS.map((_, i) => renderMOSGlobal(i, packs)).join("");

    container.innerHTML = `
        <div class="col-12">
            <div class="card pack-card">
                <div class="card-header bg-info text-white d-flex justify-content-between align-items-center">
                    <span class="pack-header">Global Status</span>
                </div>
                <div class="card-body">
                    ${global.numberOfPacks !== undefined ? `
                        <div><strong>Anzahl Packs:</strong> ${global.numberOfPacks}</div>
                        <div><strong>Globale Spannung:</strong> ${global.voltage?.toFixed(3) ?? '—'} V</div>
                        <hr>` : ""}
                    <div><strong>Mittlere Spannung:</strong> ${avg(packs.map(p => p.voltage)).toFixed(3)} V</div>
                    <div><strong>Summe Ströme:</strong> ${sum(packs.map(p => p.current)).toFixed(3)} A</div>
                    <div><strong>Mittlere Temperatur:</strong> ${avg(allTemps).toFixed(1)} °C</div>
                    <div class="mt-2"><strong>Max Zellspannung:</strong> ${maxCellVal.toFixed(3)} V (${findCellLocation(packs, maxCellVal)})</div>
                    <div><strong>Min Zellspannung:</strong> ${minCellVal.toFixed(3)} V (${findCellLocation(packs, minCellVal)})</div>
                    <hr>
                    <div><strong>MOS Status (global):</strong></div>
                    <div class="d-flex flex-wrap align-items-center mb-2">${mosStatus}</div>
                    <div class="mb-2">
                        ${["charge_on_all", "charge_off_all", "discharge_on_all", "discharge_off_all"]
                            .map(cmd => {
                                const label = cmd.includes("on") ? "EIN" : "AUS";
                                const type = cmd.includes("charge") ? "success" : "danger";
                                const action = cmd.includes("off") ? "AUS" : "EIN";
                                return `<button class="btn btn-sm btn-outline-${type} me-1"
                                    onclick="sendCommand('${cmd}')">${cmd.includes('charge') ? 'Laden' : 'Entladen'} ${action}</button>`;
                            }).join("")}
                    </div>
                    <hr>
                    <div><strong>Packs mit Fehler:</strong> ${renderPackBadges(packsWithError, 'badge bg-danger')}</div>
                    <div><strong>Packs mit Warnung:</strong> ${renderPackBadges(packsWithWarn, 'badge bg-warning text-dark')}</div>
                    <hr>
                    <div><strong>Mittlerer SOC:</strong> ${progressBar(avg(packs.map(p => p.stateOfCharge)), 'success')}</div>
                    <div class="mt-2"><strong>Mittlerer SOH:</strong> ${progressBar(avg(packs.map(p => p.stateOfHealth)), 'info')}</div>
                </div>
            </div>
        </div>
        ${packs.map(renderPackCard).join("")}
    `;
}

function renderPackCard(pack) {
    const cells = safeArr(pack.cells);
    const ntcs = safeArr(pack.ntcTemperature);
    const [maxVal, minVal] = [Math.max(...cells), Math.min(...cells)];
    const diffCell = (maxVal - minVal).toFixed(3);

    const cellsHtml = cells.map((v, i) => {
        const color = v === maxVal ? "#ff6b6b" : v === minVal ? "#4dabf7" : "#d3d3d3";
        return `<div class="cell-field" style="background:${color}">C${i + 1}: ${v?.toFixed(3) ?? "—"} V</div>`;
    }).join("");

    const mosHtml = MOS_BITS.map((t, i) => {
        const on = !!(pack.mosfetStatus & (1 << i));
        return `
            <div class="me-3 d-inline-flex align-items-center mb-1">
                <span class="${on ? "badge-mos-on" : "badge-mos-off"} me-1">${t}</span>
                <button class="btn btn-sm btn-outline-success me-1" onclick="sendCommand('${t.toLowerCase()}_on','${pack.name}')">EIN</button>
                <button class="btn btn-sm btn-outline-danger" onclick="sendCommand('${t.toLowerCase()}_off','${pack.name}')">AUS</button>
            </div>`;
    }).join("");

    const ntcHtml = ntcs.map((t, i) => `<div class="ntc-field">T${i + 1}: ${t?.toFixed(1) ?? "—"} °C</div>`).join("");

    return `
        <div class="col-lg-6 col-12">
            <div class="card pack-card">
                <div class="card-header ${getHeaderClass(STATEMACHINE_TEXT[pack.stateMachine])} d-flex justify-content-between align-items-center">
                    <span class="pack-header">${pack.name}</span>
                    <small>SOC: ${pack.stateOfCharge?.toFixed(1)}% | SOH: ${pack.stateOfHealth?.toFixed(1)}%</small>
                </div>
                <div class="card-body">
                    <div><strong>Spannung:</strong> ${pack.voltage?.toFixed(3)} V</div>
                    <div><strong>Strom:</strong> ${pack.current?.toFixed(3)} A</div>
                    <div><strong>Zyklen:</strong> ${pack.cycleCount ?? "—"}</div>
                    <div><strong>State Machine:</strong> ${STATEMACHINE_TEXT[pack.stateMachine] ?? "—"}</div>
                    <div><strong>Verfügbare Kapazität:</strong> ${pack.availableCapacity?.toFixed(1) ?? "—"} mAh</div>
                    <hr>
                    <div><strong>Alarme:</strong> ${decodeBits(pack.swAlertFlags, SWALERTFLAGS_TEXT, 'badge bg-danger')}</div>
                    <div><strong>Warnungen:</strong> ${decodeBits(pack.swWarningFlags, SWALERTFLAGS_TEXT, 'badge bg-warning text-dark')}</div>
                    <hr>
                    <div><strong>MOS Status:</strong></div>
                    <div class="d-flex flex-wrap mb-2">${mosHtml}</div>
                    <hr>
                    <div><strong>SOC:</strong></div>${progressBar(pack.stateOfCharge, 'success')}
                    <div class="mt-2"><strong>SOH:</strong></div>${progressBar(pack.stateOfHealth, 'info')}
                    <hr>
                    <div><strong>Zellen:</strong> (Δ: ${diffCell} V)</div>
                    <div class="d-flex flex-wrap mt-1">${cellsHtml}</div>
                    <hr>
                    <div><strong>NTCs:</strong></div>
                    <div class="d-flex flex-wrap">${ntcHtml}</div>
                    <hr>
                    <div><strong>Die:</strong> ${pack.dieTemperature?.toFixed(1)} °C | <strong>PVDD:</strong> ${pack.pvddVoltage?.toFixed(3)} V</div>
                </div>
            </div>
        </div>`;
}

// ---------- Command sender ----------
window.sendCommand = (cmd, pack = null) => {
    console.log("sendCommand", cmd, pack);
    fetch("/api/cmd", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ cmd, pack })
    }).catch(e => console.error("Command Error", e));
};

// ---------- Polling ----------
setInterval(fetchData, 1000);
fetchData();
