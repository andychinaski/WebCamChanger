import React from "react";
import ReactDOM from "react-dom/client";
import { Activity, Camera, CircleStop, Play, RefreshCw, Trash2 } from "lucide-react";
import { invoke } from "@tauri-apps/api/core";
import "./styles.css";

type CameraStatus = "unregistered" | "registered" | "streaming" | "stopped" | "error";

type Diagnostics = {
  camera_name: string;
  status: CameraStatus;
  output_active: boolean;
  test_frame_ready: boolean;
  events: string[];
  last_error?: string | null;
};

const initialDiagnostics: Diagnostics = {
  camera_name: "Chinaski Virtual Camera",
  status: "unregistered",
  output_active: false,
  test_frame_ready: false,
  events: ["UI loaded. Diagnostics have not been requested yet."],
  last_error: null
};

function App() {
  const [diagnostics, setDiagnostics] = React.useState<Diagnostics>(initialDiagnostics);
  const [busyAction, setBusyAction] = React.useState<string | null>(null);

  const runCommand = async (command: string) => {
    setBusyAction(command);
    try {
      const next = await invoke<Diagnostics>(command);
      setDiagnostics(next);
    } catch (error) {
      setDiagnostics((current) => ({
        ...current,
        status: "error",
        last_error: String(error),
        events: [`${command} failed: ${String(error)}`, ...current.events].slice(0, 20)
      }));
    } finally {
      setBusyAction(null);
    }
  };

  React.useEffect(() => {
    void runCommand("show_diagnostics");
  }, []);

  const actions = [
    { id: "register_virtual_camera", label: "Register virtual camera", icon: Camera },
    { id: "start_output", label: "Start output", icon: Play },
    { id: "stop_output", label: "Stop output", icon: CircleStop },
    { id: "unregister_virtual_camera", label: "Unregister virtual camera", icon: Trash2 },
    { id: "show_diagnostics", label: "Show diagnostics", icon: RefreshCw }
  ];

  return (
    <main className="min-h-screen bg-[#101216] text-zinc-100">
      <div className="grid min-h-screen grid-cols-[260px_1fr]">
        <aside className="border-r border-line bg-panel px-5 py-6">
          <div className="flex items-center gap-3">
            <div className="grid h-10 w-10 place-items-center rounded bg-accent text-slate-950">
              <Camera size={22} />
            </div>
            <div>
              <h1 className="text-base font-semibold">Chinaski</h1>
              <p className="text-xs text-zinc-400">Virtual Camera Spike</p>
            </div>
          </div>

          <nav className="mt-8 space-y-1 text-sm text-zinc-300">
            {["Dashboard", "Output", "Diagnostics"].map((item) => (
              <div key={item} className="rounded px-3 py-2 hover:bg-surface">
                {item}
              </div>
            ))}
          </nav>
        </aside>

        <section className="p-8">
          <div className="mx-auto max-w-6xl space-y-6">
            <header className="flex items-start justify-between gap-4">
              <div>
                <p className="text-sm uppercase tracking-[0.16em] text-accent">Windows 11 technical spike</p>
                <h2 className="mt-2 text-3xl font-semibold">{diagnostics.camera_name}</h2>
              </div>
              <StatusBadge status={diagnostics.status} />
            </header>

            <div className="grid grid-cols-[1fr_360px] gap-5">
              <section className="rounded border border-line bg-panel p-5">
                <div className="flex items-center justify-between">
                  <h3 className="text-sm font-medium text-zinc-300">Fallback test frame</h3>
                  <span className="text-xs text-zinc-500">1280x720 / 30 FPS target</span>
                </div>
                <div className="mt-4 grid aspect-video place-items-center rounded bg-[linear-gradient(135deg,#0f766e,#27272a_50%,#7f1d1d)]">
                  <div className="text-center">
                    <Activity className="mx-auto mb-3 text-accent" size={38} />
                    <p className="text-lg font-semibold">SPIKE FRAME PLACEHOLDER</p>
                    <p className="mt-1 text-sm text-zinc-300">Real MF frame delivery is still native-module work.</p>
                  </div>
                </div>
              </section>

              <aside className="rounded border border-line bg-panel p-5">
                <h3 className="text-sm font-medium text-zinc-300">Camera lifecycle</h3>
                <div className="mt-4 space-y-2">
                  {actions.map((action) => {
                    const Icon = action.icon;
                    return (
                      <button
                        key={action.id}
                        className="flex h-10 w-full items-center gap-3 rounded border border-line bg-surface px-3 text-left text-sm hover:border-accent disabled:cursor-wait disabled:opacity-60"
                        disabled={busyAction !== null}
                        onClick={() => void runCommand(action.id)}
                        title={action.label}
                      >
                        <Icon size={17} />
                        <span>{busyAction === action.id ? "Working..." : action.label}</span>
                      </button>
                    );
                  })}
                </div>
              </aside>
            </div>

            <section className="rounded border border-line bg-panel p-5">
              <div className="flex items-center justify-between">
                <h3 className="text-sm font-medium text-zinc-300">Diagnostics</h3>
                <span className="text-xs text-zinc-500">
                  output {diagnostics.output_active ? "active" : "inactive"} / test frame{" "}
                  {diagnostics.test_frame_ready ? "ready" : "placeholder"}
                </span>
              </div>
              {diagnostics.last_error ? (
                <p className="mt-3 rounded border border-red-500/40 bg-red-950/40 px-3 py-2 text-sm text-red-200">
                  {diagnostics.last_error}
                </p>
              ) : null}
              <div className="mt-4 max-h-64 overflow-auto rounded bg-[#0b0d10] p-3 font-mono text-xs leading-6 text-zinc-300">
                {diagnostics.events.map((event, index) => (
                  <div key={`${event}-${index}`}>{event}</div>
                ))}
              </div>
            </section>
          </div>
        </section>
      </div>
    </main>
  );
}

function StatusBadge({ status }: { status: CameraStatus }) {
  const tone = status === "streaming" ? "border-accent text-accent" : status === "error" ? "border-red-400 text-red-300" : "border-line text-zinc-300";
  return <span className={`rounded border px-3 py-1 text-sm ${tone}`}>{status}</span>;
}

ReactDOM.createRoot(document.getElementById("root") as HTMLElement).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>
);

