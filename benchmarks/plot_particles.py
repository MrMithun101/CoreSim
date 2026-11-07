#!/usr/bin/env python3
"""Optional plot of a completed particle study (requires matplotlib)."""
import argparse
import json
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.ticker import ScalarFormatter

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('study',type=Path); p.add_argument('output',type=Path)
a=p.parse_args()
rows=json.loads((a.study/'summary.json').read_text()); meta=json.loads((a.study/'metadata.json').read_text())
fig, axes=plt.subplots(1,2,figsize=(10.5,4.8),sharey=True)
for ax,layout in zip(axes,('aos','soa')):
    for i,w in enumerate([0]+meta['workers']):
        group=sorted((r for r in rows if r['layout']==layout and r['workers']==w),key=lambda r:r['particles'])
        m=[1000*r['median_ms'] for r in group]
        yerr=[[1000*(r['median_ms']-r['min_ms']) for r in group],[1000*(r['max_ms']-r['median_ms']) for r in group]]
        ax.errorbar(range(len(group)),m,yerr=yerr,capsize=4,marker=['o','s','^','D'][i%4],
                    label='Serial' if w==0 else f'{w} worker'+('s' if w!=1 else ''),lw=1.7,ms=5)
    ax.set_xticks(range(len(group)),[f"{r['particles']:,}" for r in group]); ax.set_xlabel('Particles')
    ax.set_title('Array of structures' if layout=='aos' else 'Structure of arrays',fontsize=12)
    ax.set_yscale('log'); ax.yaxis.set_major_formatter(ScalarFormatter()); ax.grid(axis='y',alpha=.25)
    ax.spines[['top','right']].set_visible(False); ax.margins(x=.09)
axes[0].set_ylabel('Microseconds per tick (log scale)')
fig.suptitle(f"CPU particle integration on {meta.get('cpu_model',meta['architecture'])}",fontsize=16,weight='bold',y=.98)
handles,labels=axes[0].get_legend_handles_labels(); fig.legend(handles,labels,ncol=len(labels),loc='upper center',bbox_to_anchor=(.5,.92),frameon=False)
fig.subplots_adjust(top=.75,bottom=.20,left=.10,right=.98,wspace=.12)
fig.text(.5,.035,f"Points: median of {meta['trials']} fresh-process trials. Bars: trial minimum–maximum. Each trial times {meta['steps']:,} ticks.",ha='center',fontsize=9)
fig.savefig(a.output,dpi=150,bbox_inches='tight'); plt.close(fig)
