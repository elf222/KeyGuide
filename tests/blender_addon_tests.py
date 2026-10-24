# tests/blender_addon_tests.py
import contextlib
import importlib.util
from pathlib import Path
import sys
import types
import unittest
from unittest.mock import patch

sys.dont_write_bytecode = True


class FakeSocket:
    def __init__(self):
        self.incoming = []
        self.sent = []
        self.closed = False

    def recvfrom(self, _):
        if not self.incoming:
            raise BlockingIOError
        return self.incoming.pop(0)

    def sendto(self, packet, address):
        self.sent.append((packet.decode("ascii"), address))

    def close(self):
        self.closed = True


def load_addon():
    bpy = types.ModuleType("bpy")
    handlers = types.ModuleType("bpy.app.handlers")
    handlers.persistent = lambda function: function
    handlers.load_post = []
    timers = types.SimpleNamespace(
        is_registered=lambda _: False, register=lambda *_, **__: None, unregister=lambda _: None
    )
    bpy.app = types.SimpleNamespace(handlers=handlers, timers=timers)
    bpy.types = types.SimpleNamespace(LayerObjects=object, Object=object, ToolSettings=object)
    bpy.msgbus = types.SimpleNamespace(clear_by_owner=lambda _: None, subscribe_rna=lambda **_: None)
    sys.modules["bpy"] = bpy
    sys.modules["bpy.app.handlers"] = handlers
    path = Path(__file__).resolve().parents[1] / "assets/blender/keyguide_bridge.py"
    spec = importlib.util.spec_from_file_location("keyguide_bridge", path)
    addon = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(addon)
    return addon, bpy


class BlenderAddonTests(unittest.TestCase):
    def setUp(self):
        self.addon, self.bpy = load_addon()
        region = types.SimpleNamespace(x=0, y=100, width=1060, height=620, type="WINDOW")
        area = types.SimpleNamespace(x=0, y=100, width=1280, height=620, type="VIEW_3D", regions=[region])
        self.window = types.SimpleNamespace(
            width=1280, height=720, screen=types.SimpleNamespace(areas=[area]), modal_operators=[]
        )
        self.bpy.context = types.SimpleNamespace(
            window=self.window,
            window_manager=types.SimpleNamespace(windows=[self.window], is_interface_locked=False),
            mode="EDIT_MESH",
            tool_settings=types.SimpleNamespace(mesh_select_mode=(True, False, True)),
            temp_override=lambda **_: contextlib.nullcontext(),
        )
        self.socket = FakeSocket()
        self.addon._socket = self.socket
        self.addon._session = "session"

    def request(self, revision=1, enabled=1, token="token"):
        return f"KC_REQUEST 1 {token} {self.addon.os.getpid()} {revision} {enabled} 400 400 1280 748".encode()

    def timer(self, now):
        with patch.object(self.addon.time, "monotonic", return_value=now):
            return self.addon._timer()

    def state_packets(self):
        return [text for text, _ in self.socket.sent if text.startswith("KC_STATE")]

    def test_read_real_properties_without_operators(self):
        self.assertEqual(
            self.addon._read_state((400, 400, 1280, 748)),
            ("EDIT_MESH", "VIEW_3D", 5, True, "NONE", False),
        )
        self.window.modal_operators = [types.SimpleNamespace(bl_idname="TRANSFORM_OT_translate")]
        self.assertEqual(self.addon._read_state((400, 400, 1280, 748))[4], "TRANSFORM_OT_translate")
        self.window.modal_operators.append(types.SimpleNamespace(bl_idname="UNKNOWN"))
        self.assertEqual(self.addon._read_state((400, 400, 1280, 748))[4], "MULTIPLE")
        del self.window.modal_operators
        self.assertEqual(self.addon._read_state((400, 400, 1280, 748))[3:5], (False, "UNAVAILABLE"))

    def test_editor_regions_and_retina_coordinates(self):
        area = self.window.screen.areas[0]
        area.regions[0].type = "UI"
        self.assertEqual(self.addon._read_state((400, 400, 1280, 748))[1], "VIEW_3D_UI")
        area.type = "NODE_EDITOR"
        self.assertEqual(self.addon._read_state((400, 400, 1280, 748))[1], "NODE_EDITOR_UI")
        self.assertIsNone(self.addon._area_under_pointer(self.window, (400, 740, 1280, 748))[0])
        area.type = "VIEW_3D"
        area.regions[0].type = "WINDOW"
        for item in [area, area.regions[0]]:
            item.x *= 2
            item.y *= 2
            item.width *= 2
            item.height *= 2
        # Window.width/height stay logical points; area/region bounds become backing pixels.
        self.assertEqual(self.addon._read_state((400, 400, 1280, 748))[1], "VIEW_3D")
        self.assertEqual(self.addon._read_state((400, 400, 300, 748))[1], "UNKNOWN")

    def test_node_editor_types_regions_and_operators(self):
        area = self.window.screen.areas[0]
        area.type = "NODE_EDITOR"
        area.spaces = types.SimpleNamespace(active=types.SimpleNamespace(tree_type="GeometryNodeTree"))
        for tree in ["GeometryNodeTree", "ShaderNodeTree", "CompositorNodeTree", "TextureNodeTree"]:
            area.spaces.active.tree_type = tree
            self.assertEqual(self.addon._read_state((400, 400, 1280, 748))[1], tree)
        self.window.modal_operators = [types.SimpleNamespace(bl_idname="NODE_OT_add_reroute")]
        self.assertEqual(self.addon._read_state((400, 400, 1280, 748))[4], "NODE_OT_add_reroute")
        area.regions[0].type = "UI"
        self.assertEqual(self.addon._read_state((400, 400, 1280, 748))[1], "NODE_EDITOR_UI")
        area.regions[0].type = "HEADER"
        self.assertEqual(self.addon._read_state((400, 400, 1280, 748))[1], "NODE_EDITOR_UI")

    def test_no_state_checks_until_active_request(self):
        with patch.object(self.addon, "_read_state", side_effect=AssertionError("state checked")):
            self.assertEqual(self.timer(10), self.addon.IDLE_INTERVAL)
            self.socket.incoming.append((self.request(enabled=0), ("127.0.0.1", self.addon.PORT)))
            self.assertEqual(self.timer(12), self.addon.IDLE_INTERVAL)

    def test_changes_heartbeats_and_input_settling(self):
        self.socket.incoming.append((self.request(), ("127.0.0.1", self.addon.PORT)))
        self.assertEqual(self.timer(10), self.addon.ACTIVE_INTERVAL)
        self.assertEqual(self.state_packets(), [])
        self.timer(10.25)
        self.assertEqual(len(self.state_packets()), 1)
        self.timer(10.5)
        self.assertEqual(len(self.state_packets()), 1)
        self.bpy.context.mode = "SCULPT"
        self.timer(10.75)
        self.assertIn(" SCULPT VIEW_3D ", self.state_packets()[-1])
        self.timer(12.75)
        self.assertEqual(len(self.state_packets()), 3)
        self.assertEqual(self.timer(16), self.addon.IDLE_INTERVAL)
        self.socket.incoming.append((self.request(revision=2), ("127.0.0.1", self.addon.PORT)))
        self.timer(17)
        count = len(self.state_packets())
        self.timer(17.25)
        self.assertEqual(len(self.state_packets()), count + 1)
        self.assertIn(" 2 SCULPT ", self.state_packets()[-1])

    def test_reject_wrong_sender_old_revision_and_bad_requests(self):
        self.socket.incoming.append((self.request(revision=4), ("127.0.0.1", self.addon.PORT)))
        self.timer(10)
        self.socket.incoming.append((self.request(revision=1), ("127.0.0.1", self.addon.PORT)))
        self.socket.incoming.append((self.request(revision=8), ("192.0.2.1", self.addon.PORT)))
        self.timer(10.5)
        self.assertEqual(self.addon._request[1], 4)
        for bad in [b"bad", b"x" * 769, self.request().replace(b"400", b"nan"),
                    self.request().replace(b"KC_REQUEST 1", b"KC_REQUEST 2"),
                    self.request(enabled=2), self.request(token="bad-token")]:
            self.assertIsNone(self.addon._parse_request(bad))
        self.socket.incoming.append((self.request(revision=1, token="new_overlay"), ("127.0.0.1", self.addon.PORT)))
        self.timer(11)
        self.assertEqual(self.addon._request[:2], ("new_overlay", 1))

    def test_unregister_closes_socket_and_cleans_handlers(self):
        self.addon.register()
        self.assertIn(self.addon._load_post, self.bpy.app.handlers.load_post)
        self.addon.unregister()
        self.assertTrue(self.socket.closed)
        self.assertEqual(self.bpy.app.handlers.load_post, [])
        self.assertIsNone(self.addon._socket)


if __name__ == "__main__":
    unittest.main()
