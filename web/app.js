const rooms = [
  ["Van",2,44],["Hallway",24,39],["Master Bedroom",45,12],["Boy's Bedroom",45,29],
  ["Bathroom",45,46],["Basement",45,68],["Basement Hallway",63,68],
  ["Right Storage Room",74,57],["Left Storage Room",74,76],["Kitchen",24,15],
  ["Living Room",4,5],["Garage",5,24],["Utility Room",5,62]
];
const roomLayouts = {
  desktop: {
    center: [8, 6],
    rooms,
  },
  mobile: {
    center: [22, 3.8],
    rooms: [
      ["Van",4,4],["Hallway",52,24],["Master Bedroom",4,36],["Boy's Bedroom",52,36],
      ["Bathroom",4,48],["Basement",52,48],["Basement Hallway",52,60],
      ["Right Storage Room",4,72],["Left Storage Room",52,72],["Kitchen",52,12],
      ["Living Room",4,12],["Garage",4,24],["Utility Room",4,60]
    ],
  },
};
const edges = [[0,1],[1,2],[1,3],[1,4],[1,5],[1,9],[5,6],[6,7],[6,8],[9,10],[9,11],[11,12]];
const roomIndexByName = new Map(rooms.map(([name], index) => [name, index]));
const byName = new Map();
const connectionLines = new Map();
const entities = new Map();
const evidence = new Map();
const house = document.querySelector("#house");
const entityLayer = document.querySelector("#entity-layer");
const connectionLayer = document.querySelector("#connections");
const log = document.querySelector("#log");
const status = document.querySelector("#status");
const progress = document.querySelector("#progress");
const eventCountDisplay = document.querySelector("#event-count");
const queueCountDisplay = document.querySelector("#queue-count");
const evidenceCountDisplay = document.querySelector("#evidence-count");
const themeToggle = document.querySelector("#theme-toggle");
const themeLabel = document.querySelector("#theme-label");
const pauseButton = document.querySelector("#pause");
const stepButton = document.querySelector("#step");
const restartButton = document.querySelector("#restart");
const cancelButton = document.querySelector("#cancel");
const explainButton = document.querySelector("#explain-run");
let queue = [], paused = false, timer = null, eventSource = null, receivedDone = false;
let renderedEvents = 0, currentSessionId = null, lastConfiguration = null, currentPersisted = false;
let observedEvents = [], currentRunInfo = null, currentShareToken = null;
let clerk = null, authEnabled = false, signedIn = false;
let activeLayout = "desktop";
const mobileLayoutQuery = window.matchMedia("(max-width: 640px)");

rooms.forEach(([name,x,y],i) => {
  const node = document.createElement("article");
  node.className = "room";
  node.dataset.name = name;
  node.dataset.index = String(i).padStart(2,"0");
  node.style.left = `${x}%`;
  node.style.top = `${y}%`;
  node.innerHTML = `<b>${name}</b><div class="room-evidence" aria-label="Evidence in ${name}"></div>`;
  house.append(node);
  byName.set(name, {node,x,y});
});
for (const [from,to] of edges) {
  const [fromName,x1,y1] = rooms[from], [toName,x2,y2] = rooms[to];
  const line = document.createElementNS("http://www.w3.org/2000/svg","line");
  line.setAttribute("x1",x1+8); line.setAttribute("y1",y1+6);
  line.setAttribute("x2",x2+8); line.setAttribute("y2",y2+6);
  line.dataset.from = fromName;
  line.dataset.to = toName;
  connectionLayer.append(line);
  connectionLines.set(connectionKey(fromName, toName), line);
}
applyRoomLayout();
initializeTheme();
if (mobileLayoutQuery.addEventListener) {
  mobileLayoutQuery.addEventListener("change", applyRoomLayout);
} else {
  mobileLayoutQuery.addListener(applyRoomLayout);
}

function connectionKey(from, to) {
  return [from, to].sort().join("::");
}

function layoutForViewport() {
  return mobileLayoutQuery.matches ? "mobile" : "desktop";
}

function applyRoomLayout() {
  activeLayout = layoutForViewport();
  const layout = roomLayouts[activeLayout];
  layout.rooms.forEach(([name,x,y]) => {
    const room = byName.get(name);
    if (!room) return;
    room.x = x;
    room.y = y;
    room.node.style.left = `${x}%`;
    room.node.style.top = `${y}%`;
  });
  updateConnectionLines();
  updateMap();
}

function updateConnectionLines() {
  const [centerX, centerY] = roomLayouts[activeLayout].center;
  for (const line of connectionLines.values()) {
    const from = byName.get(line.dataset.from);
    const to = byName.get(line.dataset.to);
    if (!from || !to) continue;
    line.setAttribute("x1", from.x + centerX);
    line.setAttribute("y1", from.y + centerY);
    line.setAttribute("x2", to.x + centerX);
    line.setAttribute("y2", to.y + centerY);
  }
}

function initializeTheme() {
  let stored = "";
  try {
    stored = localStorage.getItem("haunted-theme") || "";
  } catch {}
  const system = window.matchMedia("(prefers-color-scheme: light)").matches ? "light" : "dark";
  setTheme(stored || document.documentElement.dataset.theme || system, false);
  themeToggle?.addEventListener("click", () => {
    setTheme(document.documentElement.dataset.theme === "dark" ? "light" : "dark");
  });
}

function setTheme(theme, persist = true) {
  const nextTheme = theme === "light" ? "light" : "dark";
  document.documentElement.dataset.theme = nextTheme;
  if (persist) {
    try {
      localStorage.setItem("haunted-theme", nextTheme);
    } catch {}
  }
  if (!themeToggle || !themeLabel) return;
  const isDark = nextTheme === "dark";
  themeLabel.textContent = isDark ? "Dark" : "Light";
  themeToggle.setAttribute("aria-pressed", String(isDark));
  themeToggle.setAttribute("aria-label", `Switch to ${isDark ? "light" : "dark"} mode`);
}

function evidenceCount() {
  let count = 0;
  for (const items of evidence.values()) count += items.size;
  return count;
}

function updateProgressCopy() {
  progress.textContent = `${renderedEvents} events${queue.length ? ` - ${queue.length} queued` : ""}`;
  if (eventCountDisplay) eventCountDisplay.textContent = renderedEvents;
  if (queueCountDisplay) queueCountDisplay.textContent = queue.length;
  if (evidenceCountDisplay) evidenceCountDisplay.textContent = evidenceCount();
}

function pulseRoom(name, entity) {
  const room = byName.get(name);
  if (!room) return;
  room.node.classList.remove("pulse", "ghost-pulse");
  void room.node.offsetWidth;
  room.node.classList.add("pulse");
  room.node.classList.toggle("ghost-pulse", entity === "ghost");
  window.setTimeout(() => room.node.classList.remove("pulse", "ghost-pulse"), 700);
}

function pulseConnection(from, to) {
  if (!from || !to || !roomIndexByName.has(from) || !roomIndexByName.has(to)) return;
  const line = connectionLines.get(connectionKey(from, to));
  if (!line) return;
  line.classList.remove("connection-active");
  void line.getBoundingClientRect().width;
  line.classList.add("connection-active");
  window.setTimeout(() => line.classList.remove("connection-active"), 620);
}

function resetView() {
  entities.clear(); evidence.clear(); queue = []; receivedDone = false; renderedEvents = 0;
  currentPersisted = false; observedEvents = []; currentRunInfo = null; currentShareToken = null;
  log.innerHTML = ""; entityLayer.innerHTML = "";
  document.querySelector("#entities").innerHTML = '<p class="empty">Waiting for the first native event...</p>';
  document.querySelectorAll(".room").forEach(node => node.classList.remove("active", "pulse", "ghost-pulse"));
  document.querySelectorAll(".room-evidence").forEach(node => node.innerHTML = "");
  document.querySelectorAll("#connections line").forEach(line => line.classList.remove("connection-active"));
  updateProgressCopy();
  explainButton.disabled = true;
  document.querySelector("#explain-provider").textContent = "instant explanation - no cost";
  document.querySelector("#explain-output").innerHTML = '<p class="empty">Start or replay a simulation, then come back here for the story.</p>';
}

function renderEvidence() {
  for (const [name,{node}] of byName) {
    const items = [...(evidence.get(name) || [])];
    node.querySelector(".room-evidence").innerHTML = items.map(item =>
      `<span title="${item}">${item.slice(0,2).toUpperCase()}</span>`).join("");
  }
  updateProgressCopy();
}

function markerFor(key, entity) {
  let marker = entityLayer.querySelector(`[data-entity-key="${key}"]`);
  if (!marker) {
    marker = document.createElement("i");
    marker.dataset.entityKey = key;
    marker.className = `moving-token ${entity}`;
    marker.title = key;
    entityLayer.append(marker);
  }
  return marker;
}

function updateMap() {
  document.querySelectorAll(".room").forEach(node => node.classList.remove("active"));
  let offset = 0;
  const [centerX, centerY] = roomLayouts[activeLayout].center;
  for (const [key,entity] of entities) {
    const room = byName.get(entity.room);
    if (!room) continue;
    room.node.classList.add("active");
    const marker = markerFor(key,entity.entity);
    marker.style.left = `calc(${room.x + centerX}% + ${(offset%3)*8}px)`;
    marker.style.top = `calc(${room.y + centerY}% + ${Math.floor(offset/3)*8}px)`;
    marker.classList.toggle("exited",entity.action === "EXIT");
    offset++;
  }
}

function updateTelemetry() {
  document.querySelector("#entities").innerHTML = [...entities].map(([,e]) => {
    const fear = Math.min(100,(e.fear || 0) * 6.67);
    const boredom = Math.min(100,(e.boredom || 0) * 6.67);
    return `<article class="entity">
      <div class="entity-top"><b>${e.entity === "ghost" ? "GHOST" : `HUNTER ${e.id}`}</b><small>${e.room}</small></div>
      <small>#${e.sequence} - ${e.action} - ${e.device || "spectral"}</small>
      <div class="metric"><span>FEAR</span><div class="meter fear"><i style="width:${fear}%"></i></div><em>${e.fear || 0}</em></div>
      <div class="metric"><span>BORED</span><div class="meter boredom"><i style="width:${boredom}%"></i></div><em>${e.boredom || 0}</em></div>
    </article>`;
  }).join("");
}

function render(event) {
  if (event.done) {
    receivedDone = true;
    status.textContent = event.status === "cancelled"
      ? "CANCELLED" : event.code === 0 ? "COMPLETE" : "FAILED";
    pauseButton.disabled = true; stepButton.disabled = true;
    updateProgressCopy();
    return;
  }
  renderedEvents++;
  const key = `${event.entity}-${event.id}`;
  const previous = entities.get(key) || {};
  const room = event.action === "MOVE" ? event.extra : event.room;
  entities.set(key,{...previous,...event,room});
  if (event.action === "MOVE") pulseConnection(event.room, event.extra);
  if (event.entity === "ghost" && event.action === "EVIDENCE") {
    if (!evidence.has(event.room)) evidence.set(event.room,new Set());
    evidence.get(event.room).add(event.extra);
  }
  if (event.entity === "hunter" && event.action === "GATHER") {
    evidence.get(event.room)?.delete(event.device);
  }
  pulseRoom(room, event.entity);
  updateMap(); renderEvidence(); updateTelemetry();
  const item = document.createElement("li");
  item.className = `event-row ${event.entity} ${event.action.toLowerCase()}`;
  item.innerHTML = `<time>#${event.sequence}</time><span>${event.entity} ${event.id}</span><b>${event.action}</b><small>${event.room}${event.extra ? ` -> ${event.extra}` : ""}</small>`;
  log.prepend(item);
  updateProgressCopy();
}

function schedule() {
  clearTimeout(timer);
  if (paused || !queue.length) return;
  render(queue.shift());
  updateProgressCopy();
  timer = setTimeout(schedule, Number(document.querySelector("#speed").value));
}

function setPaused(value) {
  paused = value;
  pauseButton.textContent = paused ? "Resume" : "Pause";
  status.textContent = paused ? "PAUSED" : (receivedDone && !queue.length ? "COMPLETE" : "RUNNING");
  stepButton.disabled = !paused || !queue.length;
  if (!paused) schedule();
}

async function startSimulation() {
  if (currentSessionId && !receivedDone) {
    await fetch(`/api/sessions/${currentSessionId}`,{method:"DELETE"}).catch(()=>{});
  }
  eventSource?.close();
  clearTimeout(timer); resetView(); paused = false;
  status.textContent = "CONNECTING";
  pauseButton.textContent = "Pause";
  pauseButton.disabled = false; stepButton.disabled = true;
  restartButton.disabled = false; cancelButton.disabled = false;
  lastConfiguration = {
    seed: Number(document.querySelector("#seed").value),
    navigation: document.querySelector("#nav").value,
    scheduler: document.querySelector("#scheduler").value,
    hunters: document.querySelector("#hunters").value.split(",").map(name=>name.trim()).filter(Boolean),
  };
  try {
    const response = await fetch("/api/sessions",{
      method:"POST",
      headers:{ "content-type":"application/json", ...await authHeaders() },
      body:JSON.stringify(lastConfiguration),
    });
    if (!response.ok) throw new Error(await response.text());
    const session = await response.json();
    currentSessionId = session.id;
    currentPersisted = Boolean(session.persisted);
    currentRunInfo = { ...session, eventCount: 0, status: "running" };
    configureRunLinks(currentSessionId, currentPersisted);
    status.textContent = "RUNNING";
    await connectToSession(session.id);
  } catch (error) {
    status.textContent = "FAILED";
    const item = document.createElement("li");
    try { item.textContent = JSON.parse(error.message).error; }
    catch { item.textContent = error.message; }
    log.prepend(item);
  }
}

async function connectToSession(id) {
  eventSource?.close();
  const token = await authToken();
  const suffix = token ? `?token=${encodeURIComponent(token)}` : "";
  eventSource = new EventSource(`/api/sessions/${id}/events${suffix}`);
  eventSource.addEventListener("simulation",message => {
    const event = JSON.parse(message.data);
    observedEvents.push(event);
    queue.push(event);
    updateProgressCopy();
    if (paused) stepButton.disabled = false;
    schedule();
  });
  eventSource.addEventListener("done",message => {
    const result = JSON.parse(message.data);
    queue.push({done:true,code:result.exitCode,status:result.status});
    updateProgressCopy();
    currentRunInfo = { ...(currentRunInfo || {}), ...result, status: result.status, eventCount: result.eventCount || observedEvents.length };
    receivedDone = true;
    cancelButton.disabled = true;
    explainButton.disabled = !observedEvents.length;
    schedule();
    eventSource.close();
    loadHistory();
  });
  eventSource.addEventListener("error",() => {
    if (!receivedDone) status.textContent = "RECONNECTING";
  });
}

function configureRunLinks(id, persisted = true) {
  currentPersisted = persisted;
  if (!persisted) {
    document.querySelector("#copy-link").disabled = true;
    document.querySelector("#copy-link").dataset.url = "";
    for (const selector of ["#export-json","#export-csv"]) {
      const link = document.querySelector(selector);
      link.removeAttribute("href");
      link.classList.add("disabled"); link.setAttribute("aria-disabled","true");
    }
    return;
  }
  const replay = `${location.origin}/replay/${id}`;
  document.querySelector("#copy-link").disabled = false;
  document.querySelector("#copy-link").dataset.url = replay;
  for (const [selector,format] of [["#export-json","json"],["#export-csv","csv"]]) {
    const link = document.querySelector(selector);
    link.href = `/api/runs/${id}/export?format=${format}`;
    link.dataset.format = format;
    link.classList.remove("disabled"); link.removeAttribute("aria-disabled");
  }
}

async function loadHistory() {
  if (!signedIn) {
    document.querySelector("#history").innerHTML = '<p class="empty">Guest runs are live-only. Sign in to save and replay your simulations.</p>';
    return;
  }
  try {
    const response = await fetch("/api/runs?limit=12",{ headers: await authHeaders() });
    if (!response.ok) throw new Error("Unable to load history.");
    const {runs} = await response.json();
    document.querySelector("#history").innerHTML = runs.length
      ? runs.map(renderRunCard).join("")
      : '<p class="empty">No saved runs yet.</p>';
  } catch {
    document.querySelector("#history").innerHTML = '<p class="empty">Run history is temporarily unavailable.</p>';
  }
}

async function loadReplay(id) {
  const response = await fetch(`/api/runs/${id}`,{ headers: await authHeaders() });
  if (!response.ok) {
    status.textContent = "REPLAY NOT FOUND";
    return;
  }
  const run = await response.json();
  currentRunInfo = run;
  document.querySelector("#seed").value = run.seed;
  document.querySelector("#nav").value = run.navigation;
  document.querySelector("#scheduler").value = run.scheduler;
  document.querySelector("#hunters").value = run.hunters.join(", ");
  resetView(); currentSessionId = id; configureRunLinks(id, true);
  currentRunInfo = run; currentPersisted = true;
  status.textContent = run.status === "running" ? "LIVE REPLAY" : "REPLAY";
  pauseButton.disabled = false; restartButton.disabled = false;
  connectToSession(id);
}

async function loadSharedReplay(token) {
  const response = await fetch(`/api/shares/${token}`);
  if (!response.ok) {
    status.textContent = "SHARE NOT FOUND";
    return;
  }
  const run = await response.json();
  currentRunInfo = run;
  document.querySelector("#seed").value = run.seed;
  document.querySelector("#nav").value = run.navigation;
  document.querySelector("#scheduler").value = run.scheduler;
  document.querySelector("#hunters").value = run.hunters.join(", ");
  resetView(); currentSessionId = run.id; currentShareToken = token; configureRunLinks(run.id, false);
  currentRunInfo = run;
  status.textContent = "PUBLIC REPLAY";
  pauseButton.disabled = false; restartButton.disabled = false;
  eventSource?.close();
  eventSource = new EventSource(`/api/shares/${token}/events`);
  eventSource.addEventListener("simulation",message => {
    const event = JSON.parse(message.data);
    observedEvents.push(event);
    queue.push(event);
    updateProgressCopy();
    if (paused) stepButton.disabled = false;
    schedule();
  });
  eventSource.addEventListener("done",message => {
    const result = JSON.parse(message.data);
    queue.push({done:true,code:result.exitCode,status:result.status});
    updateProgressCopy();
    currentRunInfo = { ...(currentRunInfo || {}), ...result, status: result.status, eventCount: result.eventCount || observedEvents.length };
    receivedDone = true;
    explainButton.disabled = !observedEvents.length;
    schedule();
    eventSource.close();
  });
}

document.querySelector("#configuration").addEventListener("submit",event => {
  event.preventDefault(); startSimulation();
});
pauseButton.addEventListener("click",() => setPaused(!paused));
stepButton.addEventListener("click",() => {
  if (queue.length) render(queue.shift());
  updateProgressCopy();
  stepButton.disabled = !queue.length;
});
restartButton.addEventListener("click",startSimulation);
cancelButton.addEventListener("click",async() => {
  if (!currentSessionId) return;
  await fetch(`/api/sessions/${currentSessionId}`,{method:"DELETE", headers: await authHeaders()});
  status.textContent = "CANCELLING";
  cancelButton.disabled = true;
});
document.querySelector("#speed").addEventListener("change",() => { if (!paused) schedule(); });
document.querySelector("#clear").addEventListener("click",() => { log.innerHTML = ""; });
document.querySelector("#refresh-history").addEventListener("click",loadHistory);
document.querySelector("#history").addEventListener("click",handleHistoryAction);
explainButton.addEventListener("click",generateExplanation);
document.querySelector("#explain-audience").addEventListener("change",() => { if (!explainButton.disabled) generateExplanation(); });
document.querySelector("#explain-mode").addEventListener("change",() => { if (!explainButton.disabled) generateExplanation(); });
document.querySelector("#copy-link").addEventListener("click",async event => {
  if (!event.currentTarget.dataset.url) return;
  await navigator.clipboard.writeText(event.currentTarget.dataset.url);
  event.currentTarget.textContent = "Replay link copied";
});
for (const selector of ["#export-json","#export-csv"]) {
  document.querySelector(selector).addEventListener("click",downloadExport);
}

const replayMatch = location.pathname.match(/^\/replay\/([a-f0-9-]+)$/);
const shareMatch = location.pathname.match(/^\/share\/([A-Za-z0-9_-]+)$/);
await initializeAuth();
if (replayMatch) loadReplay(replayMatch[1]);
if (shareMatch) loadSharedReplay(shareMatch[1]);
loadHistory();

async function initializeAuth() {
  const signIn = document.querySelector("#sign-in");
  const signUp = document.querySelector("#sign-up");
  const userButton = document.querySelector("#user-button");
  try {
    const response = await fetch("/api/auth/config");
    const config = await response.json();
    authEnabled = Boolean(config.enabled && config.publishableKey && config.frontendApi);
    if (!authEnabled) {
      signIn.disabled = true; signUp.disabled = true;
      setAuthCopy("Guest mode", "Clerk keys are not configured yet. You can run simulations, but saving is offline.");
      return;
    }
    await loadClerkScripts(config.frontendApi, config.publishableKey);
    clerk = window.Clerk;
    await clerk.load({ ui: { ClerkUI: window.__internal_ClerkUICtor } });
    clerk.addListener(updateAuthState);
    signIn.addEventListener("click",() => clerk.openSignIn());
    signUp.addEventListener("click",() => clerk.openSignUp());
    clerk.mountUserButton(userButton);
    updateAuthState();
  } catch {
    signIn.disabled = true; signUp.disabled = true;
    setAuthCopy("Guest mode", "Authentication is temporarily unavailable. Simulations still run live.");
  }
}

function updateAuthState() {
  signedIn = Boolean(clerk?.user);
  document.querySelector("#sign-in").hidden = signedIn;
  document.querySelector("#sign-up").hidden = signedIn;
  document.querySelector("#user-button").hidden = !signedIn;
  setAuthCopy(
    signedIn ? "Saving enabled" : "Guest mode",
    signedIn
      ? "Runs created now are saved privately to your account history."
      : "Run simulations instantly. Sign in to save history, replays, and exports.",
  );
  loadHistory();
}

async function authHeaders() {
  const token = await authToken();
  return token ? { authorization: `Bearer ${token}` } : {};
}

async function authToken() {
  if (!authEnabled || !clerk?.session) return "";
  return await clerk.session.getToken();
}

async function downloadExport(event) {
  if (event.currentTarget.classList.contains("disabled")) return;
  event.preventDefault();
  const format = event.currentTarget.dataset.format || "json";
  const response = await fetch(event.currentTarget.href, { headers: await authHeaders() });
  if (!response.ok) return;
  const blob = await response.blob();
  const url = URL.createObjectURL(blob);
  const anchor = document.createElement("a");
  anchor.href = url;
  anchor.download = `haunted-threads-run-${currentSessionId}.${format === "csv" ? "csv" : "json"}`;
  anchor.click();
  URL.revokeObjectURL(url);
}

async function generateExplanation() {
  const audience = document.querySelector("#explain-audience").value;
  const mode = document.querySelector("#explain-mode").value;
  explainButton.disabled = true;
  document.querySelector("#explain-output").innerHTML = '<p class="empty">Generating explanation...</p>';
  try {
    const explanation = await fetchExplanation(audience, mode);
    renderExplanation(explanation);
  } catch {
    renderExplanation(localExplanation(currentRunInfo || lastConfiguration || {}, observedEvents, audience, mode));
  } finally {
    explainButton.disabled = !observedEvents.length && !currentPersisted && !currentShareToken;
  }
}

async function fetchExplanation(audience, mode) {
  const params = new URLSearchParams({ audience, mode });
  if (currentShareToken) {
    const response = await fetch(`/api/shares/${currentShareToken}/explain?${params}`);
    if (!response.ok) throw new Error("Explanation failed.");
    return await response.json();
  }
  if (currentPersisted && currentSessionId) {
    const response = await fetch(`/api/runs/${currentSessionId}/explain?${params}`, { headers: await authHeaders() });
    if (!response.ok) throw new Error("Explanation failed.");
    return await response.json();
  }
  return localExplanation(currentRunInfo || lastConfiguration || {}, observedEvents, audience, mode);
}

function renderExplanation(explanation) {
  document.querySelector("#explain-provider").textContent = explanation.provider === "ollama"
    ? "local AI explanation - no cost"
    : "instant explanation - no cost";
  const cards = (explanation.cards || []).map(card => `
    <article>
      <span>${escapeHtml(card.label)}</span>
      <b>${escapeHtml(card.value)}</b>
      <small>${escapeHtml(card.detail)}</small>
    </article>`).join("");
  const sections = Object.entries(explanation.sections || {}).map(([title,body]) => `
    <article class="explain-section">
      <b>${escapeHtml(title.replace(/([A-Z])/g, " $1"))}</b>
      <p>${escapeHtml(body)}</p>
    </article>`).join("");
  document.querySelector("#explain-output").innerHTML = `
    <div class="explain-cards">${cards}</div>
    ${sections}
  `;
}

function localExplanation(run, events, audience = "beginner", mode = "summary") {
  const analysis = localAnalyzeRun(run, events);
  return {
    provider: "template",
    audience, mode,
    cards: audience === "beginner" ? [
      { label: "Story moments", value: String(analysis.eventCount), detail: "actions in this run" },
      { label: "Clues left", value: String(analysis.evidenceCount), detail: "evidence from the ghost" },
      { label: "Route choice", value: analysis.navigation.toUpperCase(), detail: navigationDetail(analysis.navigation) },
      { label: "House activity", value: analysis.pressure, detail: "how busy the run became" },
    ] : [
      { label: "Events", value: String(analysis.eventCount), detail: "ordered native events" },
      { label: "Evidence", value: String(analysis.evidenceCount), detail: "ghost evidence events" },
      { label: "Navigation", value: analysis.navigation.toUpperCase(), detail: navigationDetail(analysis.navigation) },
      { label: "Pressure", value: analysis.pressure, detail: "shared-state activity" },
    ],
    sections: localSections(analysis, audience, mode),
  };
}

function localAnalyzeRun(run, events = []) {
  const ghostEvents = events.filter(event => event.entity === "ghost");
  const hunterEvents = events.filter(event => event.entity === "hunter");
  const evidenceEvents = ghostEvents.filter(event => event.action === "EVIDENCE");
  const gathers = hunterEvents.filter(event => event.action === "GATHER");
  const moves = hunterEvents.filter(event => event.action === "MOVE");
  const navigation = run.navigation || lastConfiguration?.navigation || "bfs";
  const eventCount = run.eventCount || events.length;
  const rooms = topLocal(events.map(event => event.action === "MOVE" && event.extra ? event.extra : event.room).filter(Boolean), 3);
  return {
    navigation,
    scheduler: run.scheduler || lastConfiguration?.scheduler || "threads",
    seed: run.seed ?? lastConfiguration?.seed,
    hunters: run.hunters || lastConfiguration?.hunters || [],
    eventCount,
    evidenceCount: evidenceEvents.length,
    gatherCount: gathers.length,
    moveCount: moves.length,
    ghostCount: ghostEvents.length,
    rooms,
    pressure: eventCount >= 100 ? "High" : eventCount >= 50 ? "Medium" : "Low",
  };
}

function localSections(analysis, audience, mode) {
  if (mode === "interviewer") {
    if (audience === "beginner") {
      return {
        "How characters act together": `The ghost and hunters created ${analysis.eventCount} moments. They can act at the same time, so the program carefully protects information they share.`,
        "How movement stays safe": "When a character moves between rooms, the program always locks rooms in the same order. That simple rule prevents everyone from getting stuck waiting on one another.",
        "What happens after the run": "The C simulation sends each action to the website, where it can be watched live, replayed, and saved by signed-in users.",
      };
    }
    return {
      concurrency: `This run generated ${analysis.eventCount} ordered events from native ghost and hunter workers, which is the entry point for discussing synchronization.`,
      locking: "Canonical locking keeps room movement deadlock-safe by acquiring shared room locks in a stable order.",
      interviewAnswer: "A strong interview explanation connects the C thread model, shared room state, sanitizer checks, Node SSE streaming, and browser replay.",
    };
  }
  if (mode === "benchmark") {
    if (audience === "beginner") {
      return {
        "Route used": `The hunters used ${analysis.navigation.toUpperCase()} navigation. ${navigationNote(analysis.navigation)}`,
        "Why compare routes": "It is like comparing a map, retracing your footsteps, and wandering. The chart shows which choice helps hunters get around with less wasted movement.",
        "A fair comparison": `Using the same starting seed (${analysis.seed ?? "unknown"}) lets the project compare route choices under repeatable conditions.`,
      };
    }
    return {
      strategy: `${analysis.navigation.toUpperCase()} was used for return navigation. ${navigationNote(analysis.navigation)}`,
      comparison: `This run had ${analysis.moveCount} hunter movement events, which gives the benchmark chart a concrete replay to explain.`,
      fairness: `Seed ${analysis.seed ?? "unknown"} keeps runs repeatable enough to compare BFS, breadcrumb, and random strategies.`,
    };
  }
  const opener = audience === "beginner"
    ? "This run shows several characters moving at the same time while the program protects shared data."
    : "This replay demonstrates a native C concurrency engine through a full-stack browser visualization.";
  if (audience === "beginner") {
    return {
      "The story": `${opener} There were ${analysis.eventCount} moments as the hunters searched the house using ${friendlyNavigation(analysis.navigation)}.`,
      "What the ghost did": `The ghost acted ${analysis.ghostCount} times and left ${analysis.evidenceCount} clue${analysis.evidenceCount === 1 ? "" : "s"} behind.`,
      "What the hunters found": `The hunters collected ${analysis.gatherCount} clue${analysis.gatherCount === 1 ? "" : "s"}. Busy areas included ${analysis.rooms.join(", ") || "the rooms they explored"}.`,
      "Why it is interesting": "Every movement you watched came from a real C simulation running several characters together, with safety rules that keep their shared world consistent.",
    };
  }
  return {
    overview: `${opener} It produced ${analysis.eventCount} events using ${analysis.scheduler} scheduling and ${analysis.navigation.toUpperCase()} navigation.`,
    ghost: `The ghost emitted ${analysis.ghostCount} actions and left ${analysis.evidenceCount} evidence event${analysis.evidenceCount === 1 ? "" : "s"}.`,
    hunters: `Hunters gathered ${analysis.gatherCount} evidence item${analysis.gatherCount === 1 ? "" : "s"}. Active rooms included ${analysis.rooms.join(", ") || "the explored house graph"}.`,
    technical: "The run matters because the app turns thread synchronization, pathfinding, event ordering, persistence, and replay into something visible.",
  };
}

function topLocal(values, limit) {
  const counts = new Map();
  for (const value of values) counts.set(value, (counts.get(value) || 0) + 1);
  return [...counts].sort((a,b) => b[1] - a[1]).slice(0, limit).map(([name]) => name);
}

function navigationDetail(navigation) {
  if (navigation === "bfs") return "shortest-path return";
  if (navigation === "breadcrumb") return "path memory return";
  return "baseline wandering";
}

function navigationNote(navigation) {
  if (navigation === "bfs") return "BFS uses the shortest known route back to the van.";
  if (navigation === "breadcrumb") return "Breadcrumb navigation retraces path history, which is useful but not always optimal.";
  return "Random navigation is the intentionally inefficient baseline.";
}

function friendlyNavigation(navigation) {
  if (navigation === "bfs") return "BFS, a shortest-path route plan";
  if (navigation === "breadcrumb") return "breadcrumbs, which retrace the path already taken";
  return "random movement, the wandering comparison route";
}

function renderRunCard(run) {
  const publicUrl = run.publicUrl ? `${location.origin}${run.publicUrl}` : "";
  const visibility = run.visibility === "public" ? "public" : "private";
  return `
    <article class="run-card" data-run-id="${escapeAttribute(run.id)}">
      <div><b>${escapeHtml(run.scheduler)} - ${escapeHtml(run.navigation)}</b><small>${new Date(run.createdAt).toLocaleString()}</small></div>
      <span class="run-status ${escapeAttribute(run.status)}">${escapeHtml(run.status)}</span>
      <small>seed ${run.seed} - ${run.eventCount} events - ${run.hunters.length} hunters</small>
      <span class="visibility-badge ${visibility}">${visibility}</span>
      <a href="/replay/${run.id}">Replay ></a>
      <div class="run-actions">
        <button type="button" data-action="${visibility === "public" ? "revoke-share" : "share"}" data-run-id="${escapeAttribute(run.id)}">
          ${visibility === "public" ? "Make private" : "Make public"}
        </button>
        <button type="button" data-action="copy-public" data-url="${escapeAttribute(publicUrl)}" ${publicUrl ? "" : "disabled"}>
          Copy public link
        </button>
      </div>
    </article>`;
}

async function handleHistoryAction(event) {
  const button = event.target.closest("button[data-action]");
  if (!button) return;
  const action = button.dataset.action;
  if (action === "copy-public") {
    if (!button.dataset.url) return;
    await navigator.clipboard.writeText(button.dataset.url);
    button.textContent = "Public link copied";
    return;
  }
  const runId = button.dataset.runId;
  if (!runId) return;
  button.disabled = true;
  try {
    if (action === "share") {
      const response = await fetch(`/api/runs/${runId}/share`, {
        method: "POST",
        headers: await authHeaders(),
      });
      if (!response.ok) throw new Error("Unable to make run public.");
      const { publicUrl } = await response.json();
      if (publicUrl) await navigator.clipboard.writeText(`${location.origin}${publicUrl}`);
    } else if (action === "revoke-share") {
      const response = await fetch(`/api/runs/${runId}/share`, {
        method: "DELETE",
        headers: await authHeaders(),
      });
      if (!response.ok) throw new Error("Unable to make run private.");
    }
    await loadHistory();
  } catch {
    button.disabled = false;
    button.textContent = "Try again";
  }
}

function escapeHtml(value) {
  return String(value).replace(/[&<>"']/g, char => ({
    "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;",
  }[char]));
}

function escapeAttribute(value) {
  return escapeHtml(value).replace(/`/g, "&#96;");
}

function setAuthCopy(title, message) {
  document.querySelector("#auth-title").textContent = title;
  document.querySelector("#auth-message").textContent = message;
}

async function loadClerkScripts(frontendApi, publishableKey) {
  await appendScript("/assets/ghlab/ui.js");
  await appendScript("/assets/ghlab/sdk.js", publishableKey);
}

function appendScript(src, publishableKey = "") {
  return new Promise((resolve, reject) => {
    const script = document.createElement("script");
    script.async = true;
    script.crossOrigin = "anonymous";
    if (publishableKey) script.dataset.clerkPublishableKey = publishableKey;
    script.src = src;
    script.addEventListener("load", resolve, { once: true });
    script.addEventListener("error", reject, { once: true });
    document.head.append(script);
  });
}
