# blender/keyguide_bridge.py
"""Read-only Blender state bridge for KeyGuide.

No operators, keymaps, modal handlers, scene writes or input injection are used.
Install this file as a Blender add-on, then enable it in Preferences.
"""

bl_info = {
    "name": "KeyGuide State Bridge",
    "author": "elf222",
    "version": (1, 1, 0),
    "blender": (4, 1, 0),
    "description": "Reports mode and running tools to the local KeyGuide",
    "category": "Interface",
}

import math
import os
import socket
import time
import uuid

import bpy
from bpy.app.handlers import persistent

HOST = "127.0.0.1"
PORT = 49538
ACTIVE_INTERVAL = 0.25
IDLE_INTERVAL = 2.0
HEARTBEAT_INTERVAL = 2.0
REQUEST_TIMEOUT = 5.0
PACKET_LIMIT = 768

_socket = None
_session = ""
_sequence = 0
_request = None
_request_at = 0.0
_hello_at = 0.0
_sent_at = 0.0
_last_sent = None
_capture_after = 0.0
_subscription_owner = object()


def _identifier(value, limit):
    return (
        isinstance(value, str)
        and 0 < len(value) <= limit
        and value.isascii()
        and all(c.isalnum() or c == "_" for c in value)
    )


def _parse_request(packet):
    if len(packet) > PACKET_LIMIT:
        return None
    try:
        fields = packet.decode("ascii").split()
        if len(fields) != 10 or fields[:2] != ["KC_REQUEST", "1"]:
            return None
        token = fields[2]
        pid, revision, enabled = map(int, fields[3:6])
        pointer = tuple(map(float, fields[6:10]))
        if (
            not _identifier(token, 32)
            or pid != os.getpid()
            or revision <= 0
            or enabled not in (0, 1)
            or not all(math.isfinite(value) and abs(value) <= 100000 for value in pointer)
        ):
            return None
        return token, revision, bool(enabled), pointer
    except (UnicodeError, ValueError):
        return None


def _area_under_pointer(window, pointer):
    """Convert the native window-relative point to Blender's client coordinates.

    The macOS receiver reports a frame rectangle including the title bar. Client
    coordinates start at its bottom edge. Areas use backing pixels on Retina,
    while Window.width is in points, so use the screen extent for hit testing.
    Hovering the overlay leaves the receiver's last Blender point unchanged.
    """
    x, y, frame_width, frame_height = pointer
    if frame_width <= 0 or frame_height <= 0 or window.width <= 0:
        return None, None
    scale = frame_width / window.width
    title_height = frame_height - window.height * scale
    if not 0 <= title_height <= 80 or not 0 <= x < frame_width:
        return None, None
    if not 0 <= y < window.height * scale:
        return None, None
    client_width = max((area.x + area.width for area in window.screen.areas), default=0)
    if client_width <= 0:
        return None, None
    x, y = x * client_width / frame_width, y * client_width / frame_width
    for area in window.screen.areas:
        if area.x <= x < area.x + area.width and area.y <= y < area.y + area.height:
            for region in area.regions:
                if (
                    region.x <= x < region.x + region.width
                    and region.y <= y < region.y + region.height
                ):
                    return area, region
            return area, None
    return None, None


def _read_state(pointer):
    windows = bpy.context.window_manager.windows
    window = bpy.context.window
    if window is None:
        window = windows[0] if len(windows) == 1 else None
    if window is None:
        return "UNKNOWN", "UNKNOWN", 0, False, "UNAVAILABLE", False
    area, region = _area_under_pointer(window, pointer)
    overrides = {"window": window}
    if area is not None:
        overrides["area"] = area
        if region is not None:
            overrides["region"] = region
    with bpy.context.temp_override(**overrides):
        mode = bpy.context.mode
        selection = sum(
            (1 << index) for index, selected in enumerate(bpy.context.tool_settings.mesh_select_mode)
            if selected
        )
        editor = "UNKNOWN"
        if area is not None:
            editor = area.type
            if area.type in {"VIEW_3D", "NODE_EDITOR"} and (region is None or region.type != "WINDOW"):
                editor = f"{area.type}_UI"
            elif area.type == "NODE_EDITOR":
                space = getattr(getattr(area, "spaces", None), "active", None)
                editor = getattr(space, "tree_type", "NODE_EDITOR") or "NODE_EDITOR"
        modal_available = hasattr(window, "modal_operators")
        operation = "UNAVAILABLE"
        if modal_available:
            operators = window.modal_operators
            operation = "NONE"
            if len(operators) == 1:
                operator = operators[0]
                operation = operator.bl_idname or operator.bl_rna.identifier
                operation = operation.replace(".", "_")
            elif operators:
                operation = "MULTIPLE"
        locked = bool(bpy.context.window_manager.is_interface_locked)
    if not _identifier(mode, 31) or not _identifier(editor, 31):
        return "UNKNOWN", "UNKNOWN", 0, modal_available, "UNKNOWN" if modal_available else "UNAVAILABLE", locked
    if not _identifier(operation, 63):
        operation = "UNKNOWN"
    return mode, editor, selection, modal_available, operation, locked


def _send(text):
    try:
        _socket.sendto(text.encode("ascii"), (HOST, PORT))
    except OSError:
        pass


def _ensure_socket():
    global _socket, _session, _sequence
    if _socket is None:
        _socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        _socket.bind((HOST, 0))
        _socket.setblocking(False)
        _session = uuid.uuid4().hex
        _sequence = 0


def _timer():
    global _request, _request_at, _hello_at, _sent_at, _sequence, _last_sent, _capture_after
    now = time.monotonic()
    try:
        _ensure_socket()
        for _ in range(16):
            try:
                packet, sender = _socket.recvfrom(PACKET_LIMIT + 1)
            except BlockingIOError:
                break
            if sender != (HOST, PORT):
                continue
            request = _parse_request(packet)
            if request is not None:
                # A new overlay token may restart its request numbering.
                if _request and request[0] == _request[0] and request[1] < _request[1]:
                    continue
                if request != _request:
                    _capture_after = now + ACTIVE_INTERVAL
                _request, _request_at = request, now
        if now - _hello_at >= HEARTBEAT_INTERVAL:
            _send(f"KC_HELLO 1 {os.getpid()} {_session}")
            _hello_at = now
        if _request is None or now - _request_at > REQUEST_TIMEOUT or not _request[2]:
            return IDLE_INTERVAL
        if now < _capture_after:
            return ACTIVE_INTERVAL
        token, revision, _, pointer = _request
        state = _read_state(pointer)
        comparison = (token, revision, state)
        if comparison != _last_sent or now - _sent_at >= HEARTBEAT_INTERVAL:
            _sequence += 1
            mode, editor, selection, modal_available, operation, locked = state
            _send(
                f"KC_STATE 1 {token} {os.getpid()} {_session} {_sequence} {revision} "
                f"{mode} {editor} {selection} {int(modal_available)} {operation} {int(locked)}"
            )
            _last_sent, _sent_at = comparison, now
        return ACTIVE_INTERVAL
    except (OSError, ReferenceError, RuntimeError, AttributeError):
        # Blender may be changing files/windows or temporarily locking its interface.
        # Retrying must not interrupt Blender's operation or synthesize a baseline.
        return IDLE_INTERVAL


def _invalidate(*_):
    global _last_sent
    _last_sent = None


def _subscribe():
    bpy.msgbus.clear_by_owner(_subscription_owner)
    for key in (
        (bpy.types.LayerObjects, "active"),
        (bpy.types.Object, "mode"),
        (bpy.types.ToolSettings, "mesh_select_mode"),
    ):
        bpy.msgbus.subscribe_rna(key=key, owner=_subscription_owner, args=(), notify=_invalidate)


@persistent
def _load_post(*_):
    _subscribe()
    _invalidate()


def register():
    _ensure_socket()
    _subscribe()
    if _load_post not in bpy.app.handlers.load_post:
        bpy.app.handlers.load_post.append(_load_post)
    if not bpy.app.timers.is_registered(_timer):
        bpy.app.timers.register(_timer, first_interval=IDLE_INTERVAL, persistent=True)


def unregister():
    global _socket, _request, _last_sent, _hello_at, _sent_at
    if bpy.app.timers.is_registered(_timer):
        bpy.app.timers.unregister(_timer)
    if _load_post in bpy.app.handlers.load_post:
        bpy.app.handlers.load_post.remove(_load_post)
    bpy.msgbus.clear_by_owner(_subscription_owner)
    if _socket is not None:
        _socket.close()
    _socket, _request, _last_sent = None, None, None
    _hello_at = _sent_at = 0.0


if __name__ == "__main__":
    register()
