import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]

class XboxGeneratedFioRoutingTest(unittest.TestCase):
    def test_function_table_maps_fileio_aliases_to_runtime_shims(self):
        source = (ROOT / "ps2xRecomp/src/lib/function_table_emitter.cpp").read_text()
        expected = {
            "fioOpen": "ps2XboxFioOpen",
            "open": "ps2XboxFioOpen",
            "sceOpen": "ps2XboxFioOpen",
            "fioRead": "ps2XboxFioRead",
            "read": "ps2XboxFioRead",
            "sceRead": "ps2XboxFioRead",
            "fioLseek": "ps2XboxFioLseek",
            "lseek": "ps2XboxFioLseek",
            "sceLseek": "ps2XboxFioLseek",
            "fioClose": "ps2XboxFioClose",
            "close": "ps2XboxFioClose",
            "sceClose": "ps2XboxFioClose",
            "fioWrite": "ps2XboxFioWrite",
            "write": "ps2XboxFioWrite",
            "sceWrite": "ps2XboxFioWrite",
        }
        for symbol, bridge in expected.items():
            with self.subTest(symbol=symbol):
                self.assertIn(f'symbol == "{symbol}"', source)
                self.assertIn(f'return "{bridge}"', source)
        self.assertIn("#ifdef PS2X_GENERATED_RUNTIME_ABI", source)
        self.assertIn("Xbox direct FileIO bridge", source)
        self.assertIn("desktop FileIO", source)

    def test_runtime_abi_has_guest_pointer_fileio_shims(self):
        source = (ROOT / "port/xbox360/ps2_generated_runtime_abi.h").read_text()
        for bridge in ("ps2XboxFioOpen", "ps2XboxFioRead", "ps2XboxFioLseek",
                      "ps2XboxFioClose", "ps2XboxFioWrite"):
            with self.subTest(bridge=bridge):
                self.assertIn(f"inline void {bridge}(", source)
        self.assertIn("dispatchIomanEe", source)
        self.assertIn("IomanCommand::Open", source)
        self.assertIn("IomanCommand::Read", source)
        self.assertIn("IomanCommand::Lseek", source)
        self.assertIn("IomanCommand::Close", source)

if __name__ == "__main__":
    unittest.main()
