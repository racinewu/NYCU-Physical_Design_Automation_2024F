import sys, os

def parse_block(path):
    with open(path, encoding='utf-8') as f:
        lines = [l.rstrip('\r\n') for l in f]
    outline_w = outline_h = 0
    blocks, terminals = [], []
    for line in lines:
        line = line.strip()
        if not line: continue
        if line.startswith('Outline:'):
            _, w, h = line.split(); outline_w, outline_h = int(w), int(h)
        elif line.startswith('NumBlocks:') or line.startswith('NumTerminals:'): continue
        elif 'terminal' in line:
            p = line.split()
            terminals.append({'name': p[0], 'x': int(p[2]), 'y': int(p[3])})
        else:
            p = line.split()
            if len(p) == 3:
                try: blocks.append({'name': p[0], 'w': int(p[1]), 'h': int(p[2])})
                except ValueError: pass
    return {'outline_w': outline_w, 'outline_h': outline_h,
            'blocks': blocks, 'terminals': terminals}

def parse_rpt(path):
    with open(path) as f:
        lines = [l.rstrip('\r\n').strip() for l in f]
    cost    = int(lines[0])
    hpwl    = int(lines[1])
    area    = int(lines[2])
    cw, ch  = map(int, lines[3].split())
    runtime = float(lines[4])
    placements = {}
    for line in lines[5:]:
        if not line: continue
        p = line.split()
        if len(p) == 5:
            x1,y1,x2,y2 = int(p[1]),int(p[2]),int(p[3]),int(p[4])
            placements[p[0]] = {'x1':x1,'y1':y1,'x2':x2,'y2':y2,
                                'w':x2-x1,'h':y2-y1,'cx':(x1+x2)//2,'cy':(y1+y2)//2}
    return {'cost':cost,'hpwl':hpwl,'area':area,
            'chip_w':cw,'chip_h':ch,'runtime':runtime,'placements':placements}

# Colors
PALETTE = [
    '#e6194b','#3cb44b','#4363d8','#f58231','#911eb4',
    '#42d4f4','#f032e6','#bfef45','#fabed4','#469990',
    '#dcbeff','#9a6324','#fffac8','#800000','#aaffc3',
    '#808000','#ffd8b1','#000075','#a9a9a9','#e8a0bf',
]

def gen_colors(n):
    return [PALETTE[i % len(PALETTE)] for i in range(n)]

# SVG builder
def build_svg(block_data, rpt_data, case_name):
    ow = block_data['outline_w']
    oh = block_data['outline_h']
    placements  = rpt_data['placements']
    terminals   = block_data['terminals']
    colors      = gen_colors(len(block_data['blocks']))
    color_map   = {b['name']: colors[i] for i,b in enumerate(block_data['blocks'])}

    # Canvas sizing
    # Extend canvas to show terminals that may be outside the outline
    PAD_L  = 60    # left  — room for y-axis coords
    PAD_R  = 60    # right
    PAD_T  = 20    # top
    PAD_B  = 50    # bottom

    all_x  = [t['x'] for t in terminals] + [ow]
    all_y  = [t['y'] for t in terminals] + [oh]
    data_w = max(all_x)
    data_h = max(all_y)

    SVG_W  = data_w + PAD_L + PAD_R
    SVG_H  = data_h + PAD_T + PAD_B

    def tx(x): return PAD_L + x
    def ty(y): return PAD_T + (data_h - y)   # flip Y

    out = []

    # SVG header
    out.append(f'<svg xmlns="http://www.w3.org/2000/svg" '
               f'width="{SVG_W}" height="{SVG_H}" '
               f'viewBox="0 0 {SVG_W} {SVG_H}" '
               f'style="background:white;font-family:sans-serif">')



    # Outer terminal area border (dashed, light grey)
    # Encloses everything including out-of-outline terminals
    out.append(f'  <rect x="{tx(0)}" y="{ty(data_h)}" '
               f'width="{data_w}" height="{data_h}" '
               f'fill="none" stroke="#bbb" stroke-width="1" stroke-dasharray="6 4"/>')

    # Outline
    out.append(f'  <rect x="{tx(0)}" y="{ty(oh)}" '
               f'width="{ow}" height="{oh}" '
               f'fill="#f9f9f9" stroke="#999" stroke-width="1.5" stroke-dasharray="8 4"/>')

    # Macros
    for b in block_data['blocks']:
        name = b['name']
        if name not in placements: continue
        p   = placements[name]
        col = color_map.get(name, '#ddd')

        # Filled rect (bounding box = placement area)
        out.append(f'  <rect x="{tx(p["x1"])}" y="{ty(p["y2"])}" '
                   f'width="{p["w"]}" height="{p["h"]}" '
                   f'fill="{col}" stroke="#333" stroke-width="0.8" opacity="0.85"/>')

        # Name label (centred)
        fs_name = max(7, min(13, min(p['w'], p['h']) // 6))
        cx_px   = tx(p['cx'])
        cy_px   = ty(p['cy'])
        out.append(f'  <text x="{cx_px}" y="{cy_px - fs_name*0.6:.1f}" '
                   f'text-anchor="middle" dominant-baseline="middle" '
                   f'font-size="{fs_name}" font-weight="bold" fill="#111" '
                   f'paint-order="stroke" stroke="white" stroke-width="2">'
                   f'{name}</text>')

        # w × h below name
        fs_dim = max(6, fs_name - 2)
        out.append(f'  <text x="{cx_px}" y="{cy_px + fs_name*0.8:.1f}" '
                   f'text-anchor="middle" dominant-baseline="middle" '
                   f'font-size="{fs_dim}" fill="#333" '
                   f'paint-order="stroke" stroke="white" stroke-width="1.5">'
                   f'{p["w"]}×{p["h"]}</text>')

        # Corner coords — lower-left (x1,y1) and upper-right (x2,y2)
        fs_coord = max(5, min(8, min(p['w'], p['h']) // 10))
        if p['w'] > 60 and p['h'] > 30:   # only draw if macro is big enough
            # lower-left corner (x1, y1)
            out.append(f'  <text x="{tx(p["x1"])+2}" y="{ty(p["y1"])-2}" '
                       f'font-size="{fs_coord}" fill="#555" '
                       f'paint-order="stroke" stroke="white" stroke-width="1.5">'
                       f'({p["x1"]},{p["y1"]})</text>')
            # upper-right corner (x2, y2)
            out.append(f'  <text x="{tx(p["x2"])-2}" y="{ty(p["y2"])+fs_coord+1}" '
                       f'text-anchor="end" '
                       f'font-size="{fs_coord}" fill="#555" '
                       f'paint-order="stroke" stroke="white" stroke-width="1.5">'
                       f'({p["x2"]},{p["y2"]})</text>')

    # Terminals
    for t in terminals:
        out.append(f'  <circle cx="{tx(t["x"])}" cy="{ty(t["y"])}" r="4" '
                   f'fill="#e94560" stroke="white" stroke-width="1"/>')
        out.append(f'  <text x="{tx(t["x"])+6}" y="{ty(t["y"])-4}" '
                   f'font-size="7" fill="#c0392b">{t["name"]}</text>')

    out.append('</svg>')
    return '\n'.join(out)


def main():
    if len(sys.argv) != 5:
        print('Usage: python3 visualizer.py <input_blk>.block <input_net>.nets <output>.rpt <output>.svg')
        sys.exit(1)

    block_file, _nets_file, rpt_file, out_svg = sys.argv[1:]
    case_name = os.path.splitext(os.path.basename(block_file))[0]

    block_data = parse_block(block_file)
    rpt_data   = parse_rpt(rpt_file)

    svg = build_svg(block_data, rpt_data, case_name)
    with open(out_svg, 'w', encoding='utf-8') as f:
        f.write(svg)

    print(f'[Visualizer] {len(block_data["blocks"])} macros, '
          f'{len(block_data["terminals"])} terminals -> {out_svg}')

if __name__ == '__main__':
    main()