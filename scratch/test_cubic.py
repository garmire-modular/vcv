from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
import resvg_py

class QuadToCubicPathPen(SVGPathPen):
    def _qCurveToOne(self, pt1, pt2):
        pt0 = self._getCurrentPoint()
        c1 = (pt0[0] + (2.0/3.0) * (pt1[0] - pt0[0]), pt0[1] + (2.0/3.0) * (pt1[1] - pt0[1]))
        c2 = (pt2[0] + (2.0/3.0) * (pt1[0] - pt2[0]), pt2[1] + (2.0/3.0) * (pt1[1] - pt2[1]))
        self.curveTo(c1, c2, pt2)

font = TTFont('res/Quicksand-Medium.ttf')
cmap = font.getBestCmap()
gset = font.getGlyphSet()

def get_cubic_path(c):
    pen = QuadToCubicPathPen(gset)
    gset[cmap[ord(c)]].draw(pen)
    return pen.getCommands()

def main():
    test_svg = '<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100"><rect width="100" height="100" fill="#7c7c7c"/>'
    for idx, char in enumerate(['E', 'Q', 'V', 'Y', 'N']):
        pd = get_cubic_path(char)
        test_svg += f'<g transform="translate({10 + idx*18}, 50) scale(0.02, -0.02)"><path d="{pd}" fill="#ffffff"/></g>'
    test_svg += '</svg>'

    png_bytes = resvg_py.svg_to_bytes(svg_string=test_svg)
    with open('scratch/cubic_test.png', 'wb') as f:
        f.write(png_bytes)
    print("Rendered scratch/cubic_test.png successfully.")

if __name__ == '__main__':
    main()
