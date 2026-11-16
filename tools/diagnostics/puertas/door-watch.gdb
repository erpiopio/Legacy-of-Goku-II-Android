set pagination off
set confirm off
set environment GBARECOMP_SELFHEAL_RECOMPILE 0
set environment GBARECOMP_INPUT_REPLAY /tmp/legacy2-rain-work/door-near.csv
break legacy2::update_extended_view
run
set $door_bus = *(unsigned long *)&'gbarecomp::g_active_bus'
set $door_oam = $door_bus + 262276 + 0x22e4
printf "door host address: %p\n", $door_oam
delete 1
watch *(unsigned int *)$door_oam
condition 2 ((*(unsigned int *)$door_oam & 0xffff) == 0xa011)
continue
printf "Door OAM write: registers R0..R15 and CPSR\n"
x/17wx &g_cpu
bt 18
quit
