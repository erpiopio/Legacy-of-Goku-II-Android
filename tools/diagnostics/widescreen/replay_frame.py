import json, os, pathlib, struct, subprocess, sys
root=pathlib.Path(__file__).resolve().parents[3]
work=pathlib.Path(os.environ.get('LEGACY2_DIAGNOSTIC_OUT','/tmp/legacy2-widescreen-replay'));work.mkdir(parents=True,exist_ok=True)
source=pathlib.Path(sys.argv[1]);tag=sys.argv[2];frames=int(sys.argv[3])
b=source.read_bytes();o=52;sections={}
for _ in range(struct.unpack_from('<I',b,48)[0]):
 t=b[o:o+4];n=struct.unpack_from('<I',b,o+4)[0];sections[t]=b[o+8:o+8+n];o+=8+n
start=struct.unpack_from('<Q',sections[b'META'],16)[0]
seq=json.loads(sys.argv[4])
replay=work/(tag+'.csv')
replay.write_text('# gbarecomp-keyinput-v1\n# frame,keyinput_active_low\n'+''.join(f'{start+int(f)},{hex(int(k))}\n' for f,k in seq))
env=os.environ.copy();env['GBARECOMP_INPUT_REPLAY']=str(replay);env['GBARECOMP_SELFHEAL_RECOMPILE']='0';env.pop('GBARECOMP_STRICT_STATIC',None)
cmd=[str(root/'build-europe/DragonBallZLegacyofGokuIIRecomp'),str(root/'game_europe.toml'),'--bios',str(root/'gbarecomp/bios/gba_bios.bin'),'--rom',str(root/'roms/legacy_of_goku_ii_europe_alfp.gba'),'--save',str(work/'progress-isolated.sav'),'--state-dir',str(work/'progress-states'),'--load-state',str(source),'--frames',str(frames),'--view-width','284','--volume','0','--dump-png',str(work/(tag+'.png')),'--dump-state',str(work/(tag+'.state'))]
with (work/(tag+'.log')).open('w') as out:
 p=subprocess.run(cmd,cwd=work,env=env,stdout=out,stderr=out,timeout=max(45,frames/20))
print('run',p.returncode,'start',start,'out',work/(tag+'.state'))
