# シンボル情報

```bash
$ arm-none-eabi-nm try-kernel | sort | less
10000000 R __boot2_start__
10000000 R boot2
10000100 R __boot2_end__
10000100 T __vector_org
10000100 T vector_tbl
100001bc T task_gsns
...
10000f68 T initsk
10000f84 T main
10000fa0 T scheduler
...
10001866 T __exidx_start
10001870 R __exidx_end
10001870 R __data_org
20000000 D __data_start
20000000 D ctsk_gsns
20000014 d init_reg_tbl
...
20000268 D __data_end
20000268 B __bss_start
20000268 B tskid_gsns
...
20001068 B semcb_tbl
200010c8 B wait_queue
200010cc B tcb_tbl
2000194c B __bss_end
2000194c B __end
```
