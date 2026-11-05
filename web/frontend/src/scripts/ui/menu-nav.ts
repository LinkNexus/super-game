/// Keyboard navigation for a menu: arrows move focus between its buttons and
/// text fields (wrapping), hovering a button focuses it so there is only
/// ever one highlight, and Escape calls the current onBack. Enter/Space/click
/// are left to the browser: a focused <button> turns them into a click, and
/// Enter in a field submits its <form>.
///
/// Inside a text field, Left/Right belong to the caret, so only Up/Down move
/// focus there; and hovering never pulls focus out of a field being typed in.
///
/// Space/Enter only press a button if they went down after `activate()`:
/// Space is also the shoot key, and a player still holding it when a menu
/// appears must not press whatever got focus.
export class MenuNav {
  private items: HTMLElement[] = [];
  private onBack?: () => void;
  /// Whether Space went down since `activate()`. Space presses a button on
  /// keyup, so a release without it is a key held from before.
  private freshSpace = false;

  constructor(private root: HTMLElement) {
    root.addEventListener("keydown", (e) => this.onKey(e));
    // Not stopped: inputs.ts needs every keyup to release `keysDown`.
    root.addEventListener("keyup", (e) => {
      if (e.code !== "Space") return;
      if (!this.freshSpace) e.preventDefault(); // held from before: no click
      this.freshSpace = false;
    });

    root.addEventListener("pointerover", (e) => {
      if (isTextField(document.activeElement)) return;
      const b = (e.target as HTMLElement).closest("button");
      if (b && this.items.includes(b)) b.focus({ preventScroll: true });
    });
  }

  /// Call whenever the container is shown or its contents change. `onBack`
  /// is per activation, since each menu page goes back somewhere else.
  activate(onBack?: () => void) {
    this.onBack = onBack;
    this.freshSpace = false;
    this.items = [
      ...this.root.querySelectorAll<HTMLElement>(
        "input:not([disabled]), button:not([disabled])",
      ),
    ];
    this.items[0]?.focus({ preventScroll: true });
  }

  /// Focuses `el` if it belongs to this menu (e.g. the first invalid field).
  focus(el: HTMLElement) {
    if (this.items.includes(el)) el.focus({ preventScroll: true });
  }

  // Keys handled here are stopped from bubbling up to the game's window
  // listeners (inputs.ts): otherwise the Escape that leaves the pause menu
  // would also reach `keysPressed` and pause again on the next frame.
  private onKey(e: KeyboardEvent) {
    const inField = isTextField(e.target as Element);
    const i = this.items.indexOf(document.activeElement as HTMLElement);
    const move = (d: number) => {
      e.preventDefault();
      e.stopPropagation();
      const next = (i + d + this.items.length) % this.items.length;
      this.items[next]?.focus({ preventScroll: true });
    };

    switch (e.code) {
      // Enter presses on keydown, Space on keyup: either way a held key's
      // repeats never count.
      case "Space":
      case "Enter":
        if (e.repeat) e.preventDefault();
        else if (e.code === "Space") this.freshSpace = true;
        break;

      case "ArrowDown":
        move(1);
        break;

      case "ArrowUp":
        move(-1);
        break;

      case "ArrowRight":
        if (!inField) move(1);
        break;

      case "ArrowLeft":
        if (!inField) move(-1);
        break;

      case "Escape":
        if (this.onBack) {
          e.preventDefault();
          e.stopPropagation();
          this.onBack();
        }
        break;
    }
  }
}

function isTextField(el: Element | null): el is HTMLInputElement {
  return el instanceof HTMLInputElement;
}
