# TimeStamp Input

## Requirement

- Supports Godot version 4.7.2 or higher.  
- You need to include Microsoft's `GameInputRedist.msi` in your deployment installer.  
- This extension is only for Windows.

## API

- `TimeStampInput.start()`
  - `TimeStampInput.start()` returns `false` in environments without GameInput Runtime.
- `TimeStampInput.stop()`
- `TimeStampInput.poll_events()`
- `TimeStampInput.get_time_usec()`


## Example

```gdscript
extends Node

var start_usec := 0

func _ready() -> void:
    TimeStampInput.start()
    start_usec = TimeStampInput.get_time_usec()

func _process(_delta: float) -> void:
    var batch = TimeStampInput.poll_events()
    for event in batch.events:
        print(event.keycode, event.pressed, start_usec - event.timestamp_usec)
```
