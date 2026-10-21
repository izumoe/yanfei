#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check the custom target's hot loops without requiring excluded engine symbols."""
from pathlib import Path
import target_budget as target

def budgets(path):
    return {p[0]:int(p[1]) for line in Path(path).read_text().splitlines()
            if (p:=line.split()) and not line.startswith('#')}

def main():
    excluded={'digital_render','digital_render_legacy','digital_render_custom','lofi_render',
              'sample_render','formant_render','wheel_render','wheel_block','grain_render',
              'grain_block','phys_render','noise_render','slice_render','slc_rev'}
    base=budgets('tests/target_budget.txt')
    new=budgets('tests/core_target_budget.txt')
    target.FUNCS=[n for n in target.FUNCS if n not in excluded]+list(new)
    base.update(target.OPTIONAL);base.update(new)
    functions=target.functions('build/felucca.dis')
    for name in target.FUNCS:
        if name not in functions:
            if name in target.OPTIONAL:continue
            raise SystemExit('core budget: missing '+name)
        stats=target.cost(functions[name])
        if stats['cost']>base[name]*(1+target.TOL):
            raise SystemExit(f'core budget: {name} exceeds {base[name]}: {stats}')
        print(f"core budget: {name} cost={stats['cost']} limit={base[name]} (+10%)")
    # Moving a loop out of line must not hide its cost from the existing ISR
    # guard. Charge keyboard, step dispatch and the new section clock at the
    # enclosing render-loop weight.
    combined=target.cost(functions['fm1_alnk0_irq'])['cost']+target.NEST*sum(
        target.cost(functions[name])['cost'] for name in ('keyboard_block','seq_tick','ts_section_tick'))
    if combined>base['fm1_alnk0_irq']*(1+target.TOL):
        raise SystemExit(f'core budget: combined ISR/key/step/section handling exceeds original limit: {combined}')
    print(f"core budget: combined ISR/key/step/section handling cost={combined} limit={base['fm1_alnk0_irq']} (+10%)")
    print('core target loop budgets passed; device audio deadlines remain unmeasured')

if __name__=='__main__':
    main()
