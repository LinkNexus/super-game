/// The game needs a keyboard until touch controls land (L83), so a phone
/// would show menus it can't get past. On a touch-only device this covers
/// the page with a notice instead. "Continue anyway" is for tablets with a
/// keyboard attached: a keyboard doesn't change what the pointer queries
/// report, so there is no way to detect one up front.

const overlay = document.getElementById("keyboard-notice")!;

function isTouchOnly() {
  return (
    matchMedia("(pointer: coarse)").matches &&
    !matchMedia("(any-pointer: fine)").matches
  );
}

export function showKeyboardNoticeIfTouchOnly() {
  if (!isTouchOnly()) return;

  const button = overlay.querySelector<HTMLButtonElement>(
    '[data-action="continue"]',
  )!;
  button.addEventListener("click", () => (overlay.hidden = true), {
    once: true,
  });

  overlay.hidden = false;
  button.focus();
}
