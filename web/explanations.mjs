const NAVIGATION_NOTES = {
  bfs: "BFS is the strongest return strategy here because it chooses the shortest known path back to the van.",
  breadcrumb: "Breadcrumb navigation follows each hunter's remembered path, which is safer than wandering but can be longer than BFS.",
  random: "Random navigation is intentionally inefficient; it shows why algorithmic pathfinding matters under pressure.",
};

const AUDIENCE_OPENERS = {
  recruiter: "This replay is a compact systems demo:",
  engineer: "From an engineering lens, this run exposes coordination between workers:",
  beginner: "In plain terms, this run shows several characters moving at the same time without breaking shared state:",
};

export async function explainRun({ run, events, audience = "beginner", mode = "summary" }) {
  const analysis = analyzeRun(run, events);
  const template = renderTemplateExplanation(analysis, audience, mode);
  if ((process.env.AI_PROVIDER || "template").toLowerCase() === "ollama") {
    const ollama = await tryOllamaExplanation(analysis, audience, mode).catch(() => null);
    if (ollama) return { ...template, provider: "ollama", sections: { ...template.sections, generated: ollama } };
  }
  return template;
}

export function analyzeRun(run, events = []) {
  const ghostEvents = events.filter(event => event.entity === "ghost");
  const hunterEvents = events.filter(event => event.entity === "hunter");
  const evidenceEvents = ghostEvents.filter(event => event.action === "EVIDENCE");
  const gathers = hunterEvents.filter(event => event.action === "GATHER");
  const exits = hunterEvents.filter(event => event.action === "EXIT");
  const moves = hunterEvents.filter(event => event.action === "MOVE");
  const busiestRooms = topCounts(events.map(event => roomFor(event)).filter(Boolean), 3);
  const ghostRooms = topCounts(ghostEvents.map(event => roomFor(event)).filter(Boolean), 3);
  const evidenceByType = topCounts(evidenceEvents.map(event => event.extra || event.device).filter(Boolean), 5);
  const gatheredByType = topCounts(gathers.map(event => event.device || event.extra).filter(Boolean), 5);
  const hunterIds = [...new Set(hunterEvents.map(event => event.id).filter(id => id != null))];
  const hunters = (run?.hunters?.length ? run.hunters : hunterIds.map(id => `Hunter ${id}`));
  const navigation = run?.navigation || "bfs";
  const scheduler = run?.scheduler || "threads";
  const eventCount = run?.eventCount || events.length;
  const moveCount = moves.length;
  const exitCount = exits.length;
  const ghostEvidenceRooms = [...new Set(evidenceEvents.map(event => event.room).filter(Boolean))];
  const concurrencyPressure = pressureLabel(eventCount, hunters.length, busiestRooms.length);
  return {
    id: run?.id || null,
    status: run?.status || "live",
    seed: run?.seed ?? null,
    scheduler,
    navigation,
    eventCount,
    moveCount,
    hunterCount: hunters.length,
    hunters,
    exitCount,
    ghostActionCount: ghostEvents.length,
    evidenceCount: evidenceEvents.length,
    gatherCount: gathers.length,
    busiestRooms,
    ghostRooms,
    evidenceByType,
    gatheredByType,
    ghostEvidenceRooms,
    concurrencyPressure,
    summary: run?.summary || null,
  };
}

export function renderTemplateExplanation(analysis, audience = "beginner", mode = "summary") {
  const safeAudience = ["recruiter", "engineer", "beginner"].includes(audience) ? audience : "beginner";
  const safeMode = ["summary", "interviewer", "benchmark"].includes(mode) ? mode : "summary";
  return {
    provider: "template",
    audience: safeAudience,
    mode: safeMode,
    title: titleFor(safeMode),
    cards: insightCards(analysis, safeAudience),
    sections: sectionsFor(analysis, safeAudience, safeMode),
  };
}

function sectionsFor(analysis, audience, mode) {
  if (mode === "interviewer") return interviewerSections(analysis, audience);
  if (mode === "benchmark") return benchmarkSections(analysis, audience);
  return summarySections(analysis, audience);
}

function summarySections(analysis, audience) {
  if (audience === "beginner") {
    return {
      "The story": `There were ${analysis.eventCount} moments as ${analysis.hunterCount} hunter${plural(analysis.hunterCount)} searched the house using ${friendlyNavigation(analysis.navigation)}.`,
      "What the ghost did": analysis.ghostRooms.length
        ? `The ghost acted ${analysis.ghostActionCount} times and was busiest around ${joinTop(analysis.ghostRooms)}. It left ${analysis.evidenceCount} clue${plural(analysis.evidenceCount)} behind.`
        : `The ghost acted ${analysis.ghostActionCount} times, but its room activity was limited in this replay.`,
      "What the hunters found": `The hunters collected ${analysis.gatherCount} clue${plural(analysis.gatherCount)}. ${analysis.exitCount ? `${analysis.exitCount} hunter exit${plural(analysis.exitCount)} ${analysis.exitCount === 1 ? "was" : "were"} also recorded.` : "No hunter exit was recorded."}`,
      "Why it is interesting": "Every movement you watched came from a real C simulation running several characters together, with safety rules that keep their shared world consistent.",
    };
  }
  return {
    overview: [
      AUDIENCE_OPENERS[audience],
      `it produced ${analysis.eventCount} ordered events with ${analysis.hunterCount} hunter${plural(analysis.hunterCount)} using ${analysis.scheduler} scheduling and ${analysis.navigation.toUpperCase()} navigation.`,
      `The ghost emitted ${analysis.ghostActionCount} actions and left ${analysis.evidenceCount} evidence event${plural(analysis.evidenceCount)}.`,
    ].join(" "),
    ghost: analysis.ghostRooms.length
      ? `The ghost was most active around ${joinTop(analysis.ghostRooms)}. Evidence appeared in ${analysis.ghostEvidenceRooms.length ? analysis.ghostEvidenceRooms.join(", ") : "the explored rooms"}.`
      : "The ghost activity was limited in the captured event stream.",
    hunters: `The hunters gathered ${analysis.gatherCount} evidence item${plural(analysis.gatherCount)} and ${analysis.exitCount} exit event${plural(analysis.exitCount)} were recorded. ${analysis.hunters.join(", ")} formed the configured team.`,
    technical: `Technically, the run matters because each emitted event comes from native simulation state that must stay synchronized while workers share rooms, evidence, fear, and movement decisions.`,
  };
}

function interviewerSections(analysis, audience) {
  if (audience === "beginner") {
    return {
      "How characters act together": `The ghost and hunters created ${analysis.eventCount} moments. They can act at the same time, so the program carefully protects information they share.`,
      "How movement stays safe": "When a character moves between rooms, the program always locks rooms in the same order. That simple rule prevents everyone from getting stuck waiting on one another.",
      "What happens after the run": "The C simulation sends each action to the website, where it can be watched live, replayed, and saved by signed-in users.",
    };
  }
  const depth = "The important systems idea is that shared room transitions must be synchronized without creating circular wait.";
  return {
    concurrency: `${depth} This replay generated ${analysis.eventCount} ordered events from ghost and hunter workers.`,
    locking: "Canonical locking prevents deadlock by acquiring room locks in a stable order instead of whichever worker happens to move first.",
    memory: "The native engine exercises dynamic structures for hunters, rooms, evidence, and path history while the service persists a replayable event stream.",
    debugging: "This project is built to be checked with unit tests, stress runs, ThreadSanitizer, and Valgrind, which turns the simulation into evidence of debugging discipline.",
    talkingPoint: `Interview answer: “I can explain this from the C thread model up through the Node replay API and browser visualization.”`,
  };
}

function benchmarkSections(analysis, audience) {
  const note = NAVIGATION_NOTES[analysis.navigation] || NAVIGATION_NOTES.bfs;
  const framing = audience === "beginner"
    ? "The navigation choice is like choosing between a map, retracing your steps, or wandering."
    : "Navigation strategy changes both movement count and risk exposure.";
  if (audience === "beginner") {
    return {
      "Route used": `The hunters used ${analysis.navigation.toUpperCase()} navigation. ${note}`,
      "Why compare routes": "It is like comparing a map, retracing your footsteps, and wandering. The chart shows which choice helps hunters get around with less wasted movement.",
      "A fair comparison": `Using ${analysis.seed == null ? "the same starting setup" : `starting seed ${analysis.seed}`} lets the project compare route choices under repeatable conditions.`,
    };
  }
  return {
    strategy: `${framing} This run used ${analysis.navigation.toUpperCase()}. ${note}`,
    fairness: `The seed${analysis.seed == null ? "" : ` ${analysis.seed}`} makes behavior reproducible enough to compare strategies across benchmark runs.`,
    comparison: "Project benchmarks compare BFS, breadcrumb, and random navigation. BFS should usually reduce return movement, while random is a deliberate baseline for wasted motion.",
    takeaway: `This replay produced ${analysis.moveCount} hunter move event${plural(analysis.moveCount)} out of ${analysis.eventCount} total events, giving the benchmark chart a concrete run to point at.`,
  };
}

function insightCards(analysis, audience) {
  if (audience === "beginner") {
    return [
      { label: "Story moments", value: String(analysis.eventCount), detail: "actions in this run" },
      { label: "Clues left", value: String(analysis.evidenceCount), detail: "evidence from the ghost" },
      { label: "Route choice", value: analysis.navigation.toUpperCase(), detail: navigationShort(analysis.navigation) },
      { label: "House activity", value: analysis.concurrencyPressure, detail: "how busy the run became" },
    ];
  }
  return [
    { label: "Events", value: String(analysis.eventCount), detail: "ordered native events" },
    { label: "Evidence", value: String(analysis.evidenceCount), detail: "ghost evidence events" },
    { label: "Navigation", value: analysis.navigation.toUpperCase(), detail: navigationShort(analysis.navigation) },
    { label: "Pressure", value: analysis.concurrencyPressure, detail: "shared-state activity" },
  ];
}

function titleFor(mode) {
  if (mode === "interviewer") return "How the system works";
  if (mode === "benchmark") return "How the routes compare";
  return "What happened in this run";
}

async function tryOllamaExplanation(analysis, audience, mode) {
  const url = process.env.OLLAMA_URL || "http://127.0.0.1:11434";
  const model = process.env.OLLAMA_MODEL || "llama3.2";
  const prompt = [
    "Explain this Haunted Threads simulation run in 120 words or fewer.",
    `Audience: ${audience}. Mode: ${mode}.`,
    "Be accurate, concise, and mention concurrency, navigation, and evidence.",
    JSON.stringify(analysis),
  ].join("\n");
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), Number(process.env.OLLAMA_TIMEOUT_MS || 2500));
  try {
    const response = await fetch(`${url.replace(/\/$/, "")}/api/generate`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ model, prompt, stream: false }),
      signal: controller.signal,
    });
    if (!response.ok) return null;
    const body = await response.json();
    return typeof body.response === "string" ? body.response.trim() : null;
  } finally {
    clearTimeout(timeout);
  }
}

function topCounts(values, limit) {
  const counts = new Map();
  for (const value of values) counts.set(value, (counts.get(value) || 0) + 1);
  return [...counts].sort((a,b) => b[1] - a[1]).slice(0, limit).map(([name,count]) => ({ name, count }));
}

function roomFor(event) {
  return event.action === "MOVE" && event.extra ? event.extra : event.room;
}

function pressureLabel(eventCount, hunterCount, activeRooms) {
  const score = eventCount + hunterCount * 8 + activeRooms * 5;
  if (score >= 120) return "High";
  if (score >= 60) return "Medium";
  return "Low";
}

function joinTop(items) {
  return items.map(item => `${item.name} (${item.count})`).join(", ");
}

function navigationShort(navigation) {
  if (navigation === "bfs") return "shortest-path return";
  if (navigation === "breadcrumb") return "path memory return";
  return "baseline wandering";
}

function friendlyNavigation(navigation) {
  if (navigation === "bfs") return "BFS, a shortest-path route plan";
  if (navigation === "breadcrumb") return "breadcrumbs, which retrace the path already taken";
  return "random movement, the wandering comparison route";
}

function plural(count) {
  return count === 1 ? "" : "s";
}
