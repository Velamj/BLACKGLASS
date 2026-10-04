"""Original BLACKGLASS articulated character and utility-van source meshes.
Uses Python's standard library only. Shapes are authored here; no external art.
OBJ units are centimeters in Z-up, +X forward. Each base part targets 100 units.
These are interim articulated static meshes, not finished skeletal production art.
"""
import hashlib
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "SourceArt" / "Characters"
VERSION = "tailored_lofts_v1"
SIDES = 24


class Mesh:
    def __init__(self):
        self.vertices, self.faces = [], []

    def vertex(self, point):
        self.vertices.append(tuple(point))
        return len(self.vertices) - 1

    def triangle(self, a, b, c):
        self.faces.append((a, b, c))

    def loft(self, profiles, exponent=3.8, center=(0, 0, 0)):
        rings = []
        for z, depth, width, offset in profiles:
            ring = []
            for index in range(SIDES):
                angle = index * math.tau / SIDES
                c, s = math.cos(angle), math.sin(angle)
                x = math.copysign(abs(c) ** (2 / exponent), c) * depth + offset
                y = math.copysign(abs(s) ** (2 / exponent), s) * width
                ring.append(self.vertex((x + center[0], y + center[1], z + center[2])))
            rings.append(ring)
        for lower, upper in zip(rings, rings[1:]):
            for i in range(SIDES):
                j = (i + 1) % SIDES
                self.triangle(lower[i], lower[j], upper[j])
                self.triangle(lower[i], upper[j], upper[i])
        for ring, reverse in ((rings[0], True), (rings[-1], False)):
            middle = tuple(sum(self.vertices[i][axis] for i in ring) / SIDES for axis in range(3))
            pole = self.vertex(middle)
            for i in range(SIDES):
                a, b = ring[i], ring[(i + 1) % SIDES]
                self.triangle(pole, b, a) if reverse else self.triangle(pole, a, b)
        return self

    def ellipse(self, center, radius):
        profiles = []
        for index in range(1, 12):
            angle = math.pi * index / 12
            profiles.append((-math.cos(angle) * radius[2], math.sin(angle) * radius[0],
                             math.sin(angle) * radius[1], 0))
        self.loft(profiles, exponent=2, center=center)
        return self

    def write(self, path, name):
        normals = [[0.0, 0.0, 0.0] for _ in self.vertices]
        for a, b, c in self.faces:
            u = [self.vertices[b][i] - self.vertices[a][i] for i in range(3)]
            v = [self.vertices[c][i] - self.vertices[a][i] for i in range(3)]
            n = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2],
                 u[0] * v[1] - u[1] * v[0])
            for vertex in (a, b, c):
                for axis in range(3):
                    normals[vertex][axis] += n[axis]
        output = ["# Original BLACKGLASS source; generator version " + VERSION, "o " + name, "s 1"]
        output += ["v %.6f %.6f %.6f" % point for point in self.vertices]
        output += ["vt %.6f %.6f" % ((math.atan2(p[1], p[0]) / math.tau) % 1, (p[2] + 50) / 100)
                   for p in self.vertices]
        for n in normals:
            length = math.sqrt(sum(value * value for value in n))
            output.append("vn %.6f %.6f %.6f" % tuple(value / max(length, 1e-12) for value in n))
        output += ["f " + " ".join(f"{i+1}/{i+1}/{i+1}" for i in face) for face in self.faces]
        path.write_text("\n".join(output) + "\n", encoding="utf-8")
        return {"source": path.relative_to(ROOT).as_posix(), "vertices": len(self.vertices),
                "triangles": len(self.faces), "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                "bounds": [[min(p[i] for p in self.vertices), max(p[i] for p in self.vertices)] for i in range(3)]}


def tailored_torso():
    # Broad shoulders, fitted waist, beveled hem, inset neck and chest.
    return Mesh().loft([(-50, 35, 38, -3), (-46, 41, 42, -2), (-22, 39, 41, 0),
        (-4, 38, 42, 1), (18, 43, 46, 0), (36, 44, 50, -2),
        (44, 37, 45, -3), (49, 25, 27, -3), (50, 22, 24, -3)], exponent=3.2)


def coat_tail():
    # Separate panels allow the existing opposing gait swings to move the hem.
    return Mesh().loft([(-50, 42, 46, -4), (-46, 48, 50, -3), (-28, 46, 49, -2),
        (0, 42, 45, 0), (30, 38, 40, 1), (47, 36, 37, 1), (50, 32, 34, 0)], exponent=5)


def head():
    mesh = Mesh().loft([(-50, 19, 24, 2), (-44, 28, 35, 5), (-31, 38, 43, 3),
        (-14, 45, 47, 0), (5, 46, 46, 0), (24, 43, 44, -2),
        (40, 34, 37, -3), (48, 19, 23, -4), (50, 8, 12, -4)], exponent=2.5)
    # Original nose bridge and ears are joined into the head mesh, not more actors.
    mesh.ellipse((45, 0, -1), (13, 8, 17))
    mesh.ellipse((-3, -47, -3), (9, 7, 18))
    mesh.ellipse((-3, 47, -3), (9, 7, 18))
    return mesh


def sleeve():
    return Mesh().loft([(-50, 35, 35, 1), (-45, 42, 42, 1), (-30, 41, 42, 0),
        (-4, 44, 44, 0), (20, 47, 48, -1), (41, 48, 50, -2),
        (49, 37, 39, -1), (50, 33, 35, -1)], exponent=2.8)


def trouser():
    return Mesh().loft([(-50, 32, 36, 0), (-45, 39, 40, 0), (-23, 40, 43, 0),
        (0, 42, 44, 1), (24, 45, 46, 0), (45, 49, 50, 0), (50, 42, 44, 0)], exponent=3.1)


def boot():
    return Mesh().loft([(-50, 40, 39, 2), (-44, 49, 48, 3), (-21, 49, 48, 3),
        (2, 44, 43, 0), (19, 33, 38, -9), (44, 30, 34, -14),
        (50, 25, 30, -14)], exponent=4.3)


def beveled_block():
    return Mesh().loft([(-50, 43, 43, 0), (-43, 50, 50, 0),
        (43, 50, 50, 0), (50, 43, 43, 0)], exponent=7)


def collar():
    return Mesh().loft([(-50, 39, 46, -4), (-35, 48, 50, -3),
        (25, 47, 46, -2), (48, 35, 37, -4), (50, 32, 34, -4)], exponent=3.3)


def hair():
    return Mesh().loft([(-50, 42, 45, -1), (-30, 48, 50, -1),
        (15, 45, 47, -3), (37, 32, 35, -5), (50, 8, 12, -6)], exponent=2.5)


def van_body():
    return Mesh().loft([(-50, 44, 44, 0), (-43, 49, 48, 0),
        (20, 50, 50, 0), (45, 48, 49, -1), (50, 45, 45, -1)], exponent=9)


def van_cabin():
    return Mesh().loft([(-50, 49, 48, 0), (-43, 50, 50, 0),
        (-6, 47, 49, -3), (35, 43, 47, -8), (46, 40, 43, -10),
        (50, 37, 39, -10)], exponent=8)


def tire():
    return Mesh().loft([(-50, 40, 40, 0), (-43, 46, 46, 0),
        (-25, 50, 50, 0), (25, 50, 50, 0), (43, 46, 46, 0),
        (50, 40, 40, 0)], exponent=2)


def selection_annulus():
    # The original marker's 100 cm diameter/height envelope keeps runtime scaling.
    # A hollow center reveals feet and terrain instead of filling the ground disk.
    mesh = Mesh()
    sides = 32
    rings = []
    for z, radius in ((-50, 50), (-50, 43), (50, 50), (50, 43)):
        rings.append([mesh.vertex((radius * math.cos(i * math.tau / sides),
                                  radius * math.sin(i * math.tau / sides), z)) for i in range(sides)])
    bottom_outer, bottom_inner, top_outer, top_inner = rings
    for i in range(sides):
        j = (i + 1) % sides
        quads = [(top_outer[i], top_outer[j], top_inner[j], top_inner[i]),
                 (bottom_outer[j], bottom_outer[i], bottom_inner[i], bottom_inner[j]),
                 (bottom_outer[i], bottom_outer[j], top_outer[j], top_outer[i]),
                 (bottom_inner[j], bottom_inner[i], top_inner[i], top_inner[j])]
        for a, b, c, d in quads:
            mesh.triangle(a, b, c)
            mesh.triangle(a, c, d)
    return mesh


def main():
    SOURCE.mkdir(parents=True, exist_ok=True)
    builders = {"SM_BG_TailoredTorso": tailored_torso, "SM_BG_CoatTail": coat_tail,
        "SM_BG_Head": head, "SM_BG_Collar": collar, "SM_BG_TrouserLeg": trouser,
        "SM_BG_Sleeve": sleeve, "SM_BG_Hair": hair, "SM_BG_Boot": boot,
        "SM_BG_BevelTrim": beveled_block, "SM_BG_VanBody": van_body,
        "SM_BG_VanCabin": van_cabin, "SM_BG_Tire": tire,
        "SM_BG_SelectionAnnulus": selection_annulus}
    report = {"generator": "Scripts/Art/Generate-Character-Meshes.py", "version": VERSION,
        "ownership": "Original geometry authored for BLACKGLASS; no third-party asset dependency.",
        "status": "Interim articulated static mesh art. Skeletal rig, final textures and animation pending.",
        "units": "centimeters; Z up; X forward; base parts nominally 100 cm", "meshes": []}
    for name, builder in builders.items():
        report["meshes"].append({"asset": "/Game/Characters/" + name,
            **builder().write(SOURCE / (name + ".obj"), name)})
    (SOURCE / "manifest.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"generated": len(report["meshes"]), "triangles": sum(m["triangles"] for m in report["meshes"]),
                      "manifest": "SourceArt/Characters/manifest.json"}))


if __name__ == "__main__":
    main()
