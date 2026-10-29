export class MenuNav {
  private buttons: HTMLButtonElement[] = [];

  constructor(
    private root: HTMLElement,
    private onBack?: () => void,
  ) {
    root.addEventListener("keydown", (e) => this.onKey(e));

    root.addEventListener("pointerover", (e) => {
      const b = (e.target as HTMLElement).closest("button");
      if (b && this.buttons.includes(b))
        b.focus({
          preventScroll: true,
        });
    });
  }

  activate() {
    this.buttons = [
      ...this.root.querySelectorAll<HTMLButtonElement>(
        "button:not([disabled])",
      ),
    ];
    this.buttons[0]?.focus({
      preventScroll: true,
    });
  }

  private onKey(e: KeyboardEvent) {
    const i = this.buttons.indexOf(document.activeElement as HTMLButtonElement);
    const move = (d: number) => {
      e.preventDefault();
      const next = (i + d + this.buttons.length) % this.buttons.length;
      this.buttons[next]?.focus({
        preventScroll: true,
      });
    };

    switch (e.code) {
      case "ArrowDown":
      case "ArrowRight":
        move(1);
        break;

      case "ArrowUp":
      case "ArrowLeft":
        move(-1);
        break;

      case "Escape":
        if (this.onBack) {
          e.preventDefault();
          this.onBack();
        }
        break;
    }
  }
}
