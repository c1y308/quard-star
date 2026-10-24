
.align 3
.section .data
.global _num_app
_num_app:
.quad 8
.quad app_0_start
.quad app_1_start
.quad app_2_start
.quad app_3_start
.quad app_4_start
.quad app_5_start
.quad app_6_start
.quad app_7_start
.quad app_7_end
.global _app_names
_app_names:
.string "initproc"
.string "thread_capacity_test"
.string "thread_main_exit"
.string "thread_process_exit"
.string "thread_test"
.string "time"
.string "user_shell"
.string "xec"
.section .data
.global app_0_start
.global app_0_end
.align 3
app_0_start:
.incbin "user/bin/initproc"
app_0_end:
.section .data
.global app_1_start
.global app_1_end
.align 3
app_1_start:
.incbin "user/bin/thread_capacity_test"
app_1_end:
.section .data
.global app_2_start
.global app_2_end
.align 3
app_2_start:
.incbin "user/bin/thread_main_exit"
app_2_end:
.section .data
.global app_3_start
.global app_3_end
.align 3
app_3_start:
.incbin "user/bin/thread_process_exit"
app_3_end:
.section .data
.global app_4_start
.global app_4_end
.align 3
app_4_start:
.incbin "user/bin/thread_test"
app_4_end:
.section .data
.global app_5_start
.global app_5_end
.align 3
app_5_start:
.incbin "user/bin/time"
app_5_end:
.section .data
.global app_6_start
.global app_6_end
.align 3
app_6_start:
.incbin "user/bin/user_shell"
app_6_end:
.section .data
.global app_7_start
.global app_7_end
.align 3
app_7_start:
.incbin "user/bin/xec"
app_7_end: