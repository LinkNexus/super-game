import { MenuNav } from "./menu-nav";

/// One button of a menu page.
export type MenuItem = {
  label: string;
  action: () => void;
  /// Shown greyed out and skipped by the arrow keys.
  disabled?: boolean;
  /// Small text under the label, e.g. "soon" on a disabled item.
  hint?: string;
  /// The pink variant, for secondary choices.
  secondary?: boolean;
};

/// One text field of a menu form.
export type MenuField = {
  /// Key of this field's value in what onSubmit receives.
  name: string;
  label: string;
  placeholder?: string;
  /// Starting value; overridden by the remembered one when `rememberAs`
  /// has one stored.
  value?: string;
  maxLength?: number;
  /// Characters that may be typed: anything else is removed as you type or
  /// paste, like the desktop text box. Tested one character at a time, so
  /// write it for a single character and without the `g` flag: /[A-Za-z_]/.
  allowed?: RegExp;
  /// Empty is refused on submit.
  required?: boolean;
  /// localStorage key: the value is pre-filled from it and saved to it on a
  /// successful submit. Per browser only; never relied on.
  rememberAs?: string;
  /// The browser's autofill hint, e.g. "nickname".
  autocomplete?: string;
};

/// Text fields above the page's buttons. Enter in a field submits.
export type MenuForm = {
  fields: MenuField[];
  submitLabel: string;
  /// Called with every field's value (trimmed) once all are valid.
  onSubmit: (values: Record<string, string>) => void;
};

/// A whole menu screen, as data: main menu, mode select, pause...
/// Which page follows which is decided by whoever builds the pages.
export type MenuPage = {
  title: string;
  subtitle?: string;
  form?: MenuForm;
  items: MenuItem[];
  /// Escape, and an automatic "Back" button at the end. Omit on pages
  /// with nowhere to go back to (the main menu).
  back?: () => void;
  doNotShowBackButton?: boolean;
  /// Only the main menu's title flickers like a neon sign.
  hero?: boolean;
};

const overlay = document.getElementById("menu-overlay")!;
const card = overlay.querySelector<HTMLElement>(".menu-card")!;
const titleEl = card.querySelector<HTMLElement>(".menu-title")!;
const subtitleEl = card.querySelector<HTMLElement>(".menu-subtitle")!;
const itemsEl = card.querySelector<HTMLElement>(".menu-items")!;
const nav = new MenuNav(overlay);

/// Replaces whatever page is showing with `page`.
export function showMenu(page: MenuPage) {
  titleEl.textContent = page.title;
  subtitleEl.textContent = page.subtitle ?? "";
  card.classList.toggle("hero", page.hero ?? false);

  const items =
    page.back && !page.doNotShowBackButton
      ? [...page.items, { label: "Back", action: page.back, secondary: true }]
      : page.items;
  const buttons = items.map(button);

  itemsEl.replaceChildren(
    ...(page.form ? [form(page.form, buttons)] : buttons),
  );

  overlay.hidden = false;
  nav.activate(page.back);
}

export function hideMenu() {
  overlay.hidden = true;
}

function button(item: MenuItem): HTMLButtonElement {
  const b = document.createElement("button");
  b.type = "button"; // never submits a form: only the form's own button does
  b.className = item.secondary ? "btn secondary" : "btn";
  b.disabled = item.disabled ?? false;
  b.textContent = item.label;

  if (item.hint) {
    const hint = document.createElement("small");
    hint.className = "btn-hint";
    hint.textContent = item.hint;
    b.append(hint);
  }

  b.addEventListener("click", item.action);
  return b;
}

/// The fields, then the submit button, then the page's other buttons, all in
/// one <form> so Enter in any field submits.
function form(spec: MenuForm, buttons: HTMLButtonElement[]): HTMLFormElement {
  const f = document.createElement("form");
  f.className = "menu menu-form";
  f.noValidate = true; // our messages, not the browser's popups

  const fields = spec.fields.map(field);

  const submit = document.createElement("button");
  submit.type = "submit";
  submit.className = "btn";
  submit.textContent = spec.submitLabel;

  f.append(...fields.map((x) => x.wrapper), submit, ...buttons);

  f.addEventListener("submit", (e) => {
    e.preventDefault();

    const invalid = fields.filter((x) => !x.validate());
    if (invalid.length > 0) {
      nav.focus(invalid[0]!.input);
      return;
    }

    const values: Record<string, string> = {};
    for (const x of fields) {
      values[x.spec.name] = x.input.value.trim();
      if (x.spec.rememberAs) remember(x.spec.rememberAs, values[x.spec.name]!);
    }
    spec.onSubmit(values);
  });

  return f;
}

function field(spec: MenuField) {
  const wrapper = document.createElement("label");
  wrapper.className = "field";

  const labelEl = document.createElement("span");
  labelEl.className = "field-label";
  labelEl.textContent = spec.label;

  const input = document.createElement("input");
  input.type = "text";
  input.name = spec.name;
  input.placeholder = spec.placeholder ?? "";
  input.spellcheck = false;
  input.autocomplete = (spec.autocomplete ?? "off") as AutoFill;
  if (spec.maxLength) input.maxLength = spec.maxLength;
  input.value = filter(
    (spec.rememberAs && recall(spec.rememberAs)) || spec.value || "",
    spec,
  );

  const errorEl = document.createElement("span");
  errorEl.className = "field-error";

  // Filter as you type and paste, keeping the caret where it was.
  input.addEventListener("input", (e) => {
    if ((e as InputEvent).isComposing) return; // let an IME finish first
    const before = input.value;
    const caret = input.selectionStart ?? before.length;
    const after = filter(before, spec);
    if (after !== before) {
      const removedBeforeCaret =
        caret - filter(before.slice(0, caret), spec).length;
      input.value = after;
      const pos = caret - removedBeforeCaret;
      input.setSelectionRange(pos, pos);
    }
    if (wrapper.classList.contains("invalid")) validate(); // clear once fixed
  });

  const validate = () => {
    const message =
      spec.required && input.value.trim() === "" ? "Required" : "";
    errorEl.textContent = message;
    wrapper.classList.toggle("invalid", message !== "");
    input.setAttribute("aria-invalid", String(message !== ""));
    return message === "";
  };

  wrapper.append(labelEl, input, errorEl);
  return { spec, wrapper, input, validate };
}

function filter(value: string, spec: MenuField): string {
  let out = spec.allowed
    ? [...value].filter((c) => spec.allowed!.test(c)).join("")
    : value;
  if (spec.maxLength) out = out.slice(0, spec.maxLength);
  return out;
}

// localStorage can be missing or throw (private windows, blocked storage);
// a remembered name is a convenience, so failures are just ignored.
function recall(key: string): string {
  try {
    return localStorage.getItem(key) ?? "";
  } catch {
    return "";
  }
}

function remember(key: string, value: string) {
  try {
    localStorage.setItem(key, value);
  } catch {
    // ignored: see recall()
  }
}
