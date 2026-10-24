// State object that produces output matching the controller model
const controllerState = {
  throttle: 0,       // Integer: -100 to 100
  left: false,       // Boolean
  right: false       // Boolean
};

// UI Element References
const sliderTrack = document.getElementById('slider-track');
const sliderThumb = document.getElementById('slider-thumb');
const btnLeft = document.getElementById('btn-left');
const btnRight = document.getElementById('btn-right');
const statusDisplay = document.getElementById('status-display');

let isDragging = false;
let trackRect = null;

// Recalculate dimensions dynamically on resize/orientation change
function updateLayout() {
  trackRect = sliderTrack.getBoundingClientRect();
  positionThumb(controllerState.throttle);
}

window.addEventListener('resize', updateLayout);
window.addEventListener('orientationchange', () => {
  setTimeout(updateLayout, 100);
});

// Update the thumb's CSS position based on a -100 to 100 value
function positionThumb(value) {
  if (!trackRect) trackRect = sliderTrack.getBoundingClientRect();
  const trackHeight = trackRect.height;
  
  // Value 100 (top, 0% offset), 0 (center, 50% offset), -100 (bottom, 100% offset)
  const percentOffset = 50 - (value / 2);
  sliderThumb.style.top = `${percentOffset}%`;
}

// Convert touch/pointer coordinate into clamped -100 to 100 integer
function handleSliderMove(clientY) {
  if (!trackRect) trackRect = sliderTrack.getBoundingClientRect();
  
  const trackTop = trackRect.top;
  const trackHeight = trackRect.height;
  
  // Normalized 0.0 (top) to 1.0 (bottom)
  let normalized = (clientY - trackTop) / trackHeight;
  normalized = Math.max(0, Math.min(1, normalized));

  // Map 0.0 -> 100, 0.5 -> 0, 1.0 -> -100
  let throttleValue = Math.round((0.5 - normalized) * 200);

  // Apply a small deadzone around center
  if (Math.abs(throttleValue) < 4) {
    throttleValue = 0;
  }

  controllerState.throttle = throttleValue;
  positionThumb(controllerState.throttle);
  notifyStateChange();
}

// Slider Pointer Events (Self-centering on release)
sliderTrack.addEventListener('pointerdown', (e) => {
  isDragging = true;
  sliderTrack.setPointerCapture(e.pointerId);
  handleSliderMove(e.clientY);
});

sliderTrack.addEventListener('pointermove', (e) => {
  if (isDragging) {
    handleSliderMove(e.clientY);
  }
});

function releaseSlider(e) {
  if (!isDragging) return;
  isDragging = false;
  try {
    sliderTrack.releasePointerCapture(e.pointerId);
  } catch (err) {}
  
  // Spring back to 0 (Coast/Brake)
  controllerState.throttle = 0;
  positionThumb(0);
  notifyStateChange();
}

sliderTrack.addEventListener('pointerup', releaseSlider);
sliderTrack.addEventListener('pointercancel', releaseSlider);

// Mutually Exclusive Steering Handlers
function setSteering(targetDirection) {
  if (targetDirection === 'left') {
    controllerState.left = true;
    controllerState.right = false;
    btnLeft.classList.add('active');
    btnRight.classList.remove('active');
  } else if (targetDirection === 'right') {
    controllerState.left = false;
    controllerState.right = true;
    btnRight.classList.add('active');
    btnLeft.classList.remove('active');
  } else {
    controllerState.left = false;
    controllerState.right = false;
    btnLeft.classList.remove('active');
    btnRight.classList.remove('active');
  }
  notifyStateChange();
}

// Button Listeners (Pointer Events support touch + mouse cleanly)
btnLeft.addEventListener('pointerdown', (e) => {
  btnLeft.setPointerCapture(e.pointerId);
  setSteering('left');
});
btnLeft.addEventListener('pointerup', () => setSteering('center'));
btnLeft.addEventListener('pointercancel', () => setSteering('center'));

btnRight.addEventListener('pointerdown', (e) => {
  btnRight.setPointerCapture(e.pointerId);
  setSteering('right');
});
btnRight.addEventListener('pointerup', () => setSteering('center'));
btnRight.addEventListener('pointercancel', () => setSteering('center'));

// State Reporting Callback
function notifyStateChange() {
  const steerStr = controllerState.left ? "LEFT" : (controllerState.right ? "RIGHT" : "CENTER");
  statusDisplay.textContent = `THROTTLE: ${controllerState.throttle}% | STEER: ${steerStr}`;

  // Log output structure:
  // { throttle: -100 to 100, left: bool, right: bool }
  console.log("State:", JSON.stringify(controllerState));
}

// Initial Layout Computation
window.addEventListener('DOMContentLoaded', updateLayout);