#pragma once

namespace SimCopterLookInput
{
// Mouse axes already contain the displacement accumulated since the last input pass.
// Keep the established sensitivity at 60 FPS without scaling that displacement a second
// time on long frames. Keyboard/stick axes remain rates and still use elapsed time.
constexpr float MouseReferenceSeconds = 1.0f / 60.0f;
}
