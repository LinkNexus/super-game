/// Short notifications in a corner: info, success, error. They never take
/// focus, so they can pop up over menus or a running match without stealing
/// keys. Click one to dismiss it; hovering pauses its timer.

export type ToastKind = "info" | "success" | "error";

export type ToastOptions = {
  kind?: ToastKind;
  /// Bold first line, e.g. "Connection lost".
  title?: string;
  /// 0 keeps it until clicked. Defaults: 4 s, errors 7 s.
  durationMs?: number;
};

type Toast = { el: HTMLElement; dismiss: () => void; restart: () => void };

const MAX_VISIBLE = 4;
const DEFAULT_MS: Record<ToastKind, number> = {
  info: 4000,
  success: 4000,
  error: 7000,
};

const container = document.getElementById("toasts")!;
/// Visible toasts by kind + title + text, oldest first (Map keeps order).
const visible = new Map<string, Toast>();

/// Shows a toast and returns a function that dismisses it early. Showing
/// the same text again while it's still up restarts that one instead of
/// stacking a copy (a socket's `error` is always followed by its `close`).
export function showToast(message: string, options: ToastOptions = {}) {
  const kind = options.kind ?? "info";
  const key = `${kind}|${options.title ?? ""}|${message}`;

  const existing = visible.get(key);
  if (existing) {
    existing.restart();
    return existing.dismiss;
  }

  const toast = createToast(
    key,
    message,
    kind,
    options.title,
    options.durationMs ?? DEFAULT_MS[kind],
  );
  visible.set(key, toast);
  container.append(toast.el);

  for (const [, oldest] of visible) {
    if (visible.size <= MAX_VISIBLE) break;
    oldest.dismiss();
  }

  return toast.dismiss;
}

function createToast(
  key: string,
  message: string,
  kind: ToastKind,
  title: string | undefined,
  durationMs: number,
): Toast {
  const el = document.createElement("div");
  el.className = `toast ${kind}`;
  // Errors interrupt a screen reader; the rest wait their turn.
  el.setAttribute("role", kind === "error" ? "alert" : "status");

  if (title) {
    const t = document.createElement("strong");
    t.className = "toast-title";
    t.textContent = title;
    el.append(t);
  }

  const m = document.createElement("span");
  m.className = "toast-message";
  m.textContent = message;
  el.append(m);

  let timer: number | undefined;
  const stop = () => window.clearTimeout(timer);
  const start = () => {
    stop();
    if (durationMs > 0) timer = window.setTimeout(dismiss, durationMs);
  };

  const dismiss = () => {
    stop();
    el.remove();
    visible.delete(key);
  };

  const restart = () => {
    // Replays the entrance so a repeat is noticed.
    el.classList.remove("again");
    void el.offsetWidth;
    el.classList.add("again");
    start();
  };

  el.addEventListener("click", dismiss);
  el.addEventListener("pointerenter", stop);
  el.addEventListener("pointerleave", start);

  start();
  return { el, dismiss, restart };
}
