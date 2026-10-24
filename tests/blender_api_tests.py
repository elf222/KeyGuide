# tests/blender_api_tests.py
"""Run with Blender --factory-startup --background --python tests/blender_api_tests.py.

Only this disposable factory scene is changed to verify actual mode transitions.
The shipped add-on itself never calls bpy.ops or changes Blender data.
"""
import importlib.util
import os
from pathlib import Path
import socket
import select
import sys
import time

import bpy

assert bpy.app.background and not bpy.data.filepath, "Use a disposable factory-startup process"
sys.dont_write_bytecode = True
path = Path(__file__).resolve().parents[1] / "assets/blender/keyguide_bridge.py"
spec = importlib.util.spec_from_file_location("keyguide_bridge", path)
addon = importlib.util.module_from_spec(spec)
spec.loader.exec_module(addon)

receiver = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
receiver.bind(("127.0.0.1", 0))
receiver.settimeout(2)
addon.PORT = receiver.getsockname()[1]


def receive_state():
    while True:
        packet, _ = receiver.recvfrom(768)
        fields = packet.decode("ascii").split()
        if fields[0] == "KC_STATE":
            return fields


try:
    before = tuple(tuple(vertex.co) for vertex in bpy.context.object.data.vertices)
    addon.register()
    assert bpy.app.timers.is_registered(addon._timer)
    assert addon._timer() == addon.IDLE_INTERVAL
    packet, address = receiver.recvfrom(768)
    assert packet.decode().startswith(f"KC_HELLO 1 {os.getpid()} ")
    receiver.sendto(f"KC_REQUEST 1 fixture {os.getpid()} 1 1 0 0 0 0".encode(), address)
    assert select.select([addon._socket], [], [], 1)[0]
    interval = addon._timer()
    assert interval == addon.ACTIVE_INTERVAL, (interval, addon._request, addon._socket.getsockname(), address, addon.PORT)
    time.sleep(addon.ACTIVE_INTERVAL + 0.01)
    addon._timer()
    observed = receive_state()
    assert observed[7] == "OBJECT" and observed[8] == "UNKNOWN"
    assert observed[10] == str(int(hasattr(bpy.context.window, "modal_operators")))
    assert tuple(tuple(vertex.co) for vertex in bpy.context.object.data.vertices) == before
    # Controlled changes belong to this test fixture, not to the add-on.
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.context.tool_settings.mesh_select_mode = (True, False, True)
    addon._timer()
    observed = receive_state()
    assert observed[7] == "EDIT_MESH" and observed[9] == "5"
    bpy.ops.object.mode_set(mode="OBJECT")
    bpy.ops.object.mode_set(mode="SCULPT")
    addon._timer()
    assert receive_state()[7] == "SCULPT"
    bpy.ops.object.mode_set(mode="OBJECT")
    addon._load_post(None)
    addon._timer()
    assert receive_state()[7] == "OBJECT"
    assert bpy.app.timers.is_registered(addon._timer)
    bpy.ops.curve.primitive_bezier_curve_add()
    bpy.ops.object.mode_set(mode="EDIT")
    points = bpy.context.object.data.splines[0].bezier_points
    before_curve = tuple((tuple(point.co), tuple(point.handle_left), tuple(point.handle_right)) for point in points)
    addon._timer()
    assert receive_state()[7] == "EDIT_CURVE"
    assert tuple((tuple(point.co), tuple(point.handle_left), tuple(point.handle_right)) for point in points) == before_curve
    bpy.ops.object.mode_set(mode="OBJECT")
    addon._timer()
    assert receive_state()[7] == "OBJECT"
    window = bpy.context.window
    area = next(area for area in window.screen.areas if area.type == "VIEW_3D")
    area.type = "NODE_EDITOR"
    region = next(region for region in area.regions if region.type == "WINDOW")
    # A background process has no native mouse location. Select a real test area directly;
    # coordinate / region hit testing is covered by the separate add-on unit fixture.
    original_area_lookup = addon._area_under_pointer
    addon._area_under_pointer = lambda *_: (area, region)
    tree = bpy.data.node_groups.new("KeyGuide disposable fixture", "GeometryNodeTree")
    first = tree.nodes.new("NodeReroute")
    second = tree.nodes.new("NodeReroute")
    tree.links.new(first.outputs[0], second.inputs[0])
    before_nodes = (len(tree.nodes), len(tree.links), tuple(tuple(node.location) for node in tree.nodes))
    for tree_type in ["GeometryNodeTree", "ShaderNodeTree", "CompositorNodeTree"]:
        area.spaces.active.tree_type = tree_type
        addon._timer()
        assert receive_state()[8] == tree_type
    assert (len(tree.nodes), len(tree.links), tuple(tuple(node.location) for node in tree.nodes)) == before_nodes
    ui_region = next(region for region in area.regions if region.type == "UI")
    addon._area_under_pointer = lambda *_: (area, ui_region)
    addon._timer()
    assert receive_state()[8] == "NODE_EDITOR_UI"
    addon._area_under_pointer = original_area_lookup
    print("Actual Blender registration, UDP handshake, mesh / curve / node editor reads and reload checks passed")
    print("Blender version:", bpy.app.version_string)
finally:
    addon.unregister()
    receiver.close()
    assert not bpy.app.timers.is_registered(addon._timer) and addon._socket is None
