/*
 *** Try Kernel
 *      システムライブラリ
 */
#include <trykernel.h>
#include <knldef.h>

/* UART0の初期化 */
void tm_com_init(void)
{
    out_w(UART0_BASE+UARTx_IBRD, 67);                               /* ボーレート設定 */
    out_w(UART0_BASE+UARTx_FBRD, 52);
    out_w(UART0_BASE+UARTx_LCR_H, 0x70);                            /* データ形式設定 */
    out_w(UART0_BASE+UARTx_CR, UART_CR_RXE|UART_CR_TXE|UART_CR_EN); /* 通信イネーブル */
}


/* デバッグ用UART出力 */
UINT tm_putstring(char* str)
{
    UINT	cnt = 0;

    while(*str) {
        while((in_w(UART0_BASE+UARTx_FR) & UART_FR_TXFF)!= 0);  /* 送信FIFOの空き待ち */
        out_w(UART0_BASE+UARTx_DR, *str++);                     /* データ送信 */
        cnt++;
    }
    return cnt;
}

/*
// デバッグ用UART出力
UINT tm_puterr(ER err)
{
    const UB *EMES[] = {
        [0] = "OK",
        [5] = "SYS",
        [6] = "NOCP",
        [9] = "NOSPT",
        [10] = "RSBN",
        [11] = "RSATR",
        [17] = "PAR",
        [18] = "ID",
        [25] = "CTX",
        [27] = "OACV",
        [28] = "ILUSE",
        [33] = "NOMEM",
        [34] = "LIMIT",
        [41] = "OBJ",
        [42] = "NOEXS",
        [43] = "QOVR",
        [49] = "RLWAI",
        [50] = "TMOUT",
        [51] = "DLT",
        [52] = "DISWAI",
        [57] = "IO",
    };

    UINT	cnt = 0;

    err *= -1;
    UB *str = EMES[err];
    while(*str) {
        while((in_w(UART0_BASE+UARTx_FR) & UART_FR_TXFF)!= 0);  // 送信FIFOの空き待ち
        out_w(UART0_BASE+UARTx_DR, *str++);                     // データ送信
        cnt++;
    }
    return cnt;
}
*/
