#include <trykernel.h>
#include "dev_i2c.h"
#include "i2c_sysdep.h"

/*----------------------------------------------------------------------
 * レジスタ・ベースアドレス
*/
const static UW ba[DEV_I2C_UNITNM] = { I2C0_BASE, I2C1_BASE };

/*----------------------------------------------------------------------
 * デバイス制御データ
*/
typedef struct {
    ID      semid;      /* セマフォID */
    UW      init;       /* 初期化フラグ */
    UINT    omode;      /* オープンモード */
    UW      sadr;       /* スレーブアドレス */
    UW      sdat_num;   /* 送信データ数 */
    UW      rdat_num;   /* 受信データ数 */
    UB      *sbuf;      /* 送信バッファ*/
    UB      *rbuf;      /* 受信バッファ */
} T_I2C_LLDCB;
static T_I2C_LLDCB    ll_devcb[DEV_I2C_UNITNM];

/* ドライバ状態 */
#define    I2C_STS_START       0x0000
#define    I2C_STS_RESTART     0x0001
#define    I2C_STS_STOP        0x0003
#define    I2C_STS_SEND        0x0004
#define    I2C_STS_RECV        0x0005
#define    I2C_STS_TOP         0x8000

/*----------------------------------------------------------------------
 * デバイス初期化
 */
static void i2c_init(UW unit)
{
    switch(unit) {
    case 0:
        set_w( RESETS_RESET, RESETS_RESET_I2C0);
        clr_w( RESETS_RESET, RESETS_RESET_I2C0);
        while((in_w(RESETS_RESET_DONE)&(RESETS_RESET_I2C0))==0);

        out_w(GPIO_CTRL(I2C_SDA_0), GPIO_CTRL_FUNCSEL_I2C);
        out_w(GPIO(I2C_SDA_0), GPIO_IE | GPIO_DRIVE_4MA | GPIO_PUE | GPIO_SHEMITT);
        out_w(GPIO_CTRL(I2C_SCL_0), GPIO_CTRL_FUNCSEL_I2C);
        out_w(GPIO(I2C_SCL_0), GPIO_IE | GPIO_DRIVE_4MA | GPIO_PUE | GPIO_SHEMITT);
        break;
    case 1:
        set_w( RESETS_RESET, RESETS_RESET_I2C1);
        clr_w( RESETS_RESET, RESETS_RESET_I2C1);
        while((in_w(RESETS_RESET_DONE)&(RESETS_RESET_I2C1))==0);

        out_w(GPIO_CTRL(I2C_SDA_1), GPIO_CTRL_FUNCSEL_I2C);
        out_w(GPIO(I2C_SDA_1), GPIO_IE | GPIO_DRIVE_4MA | GPIO_PUE | GPIO_SHEMITT);
        out_w(GPIO_CTRL(I2C_SCL_1), GPIO_CTRL_FUNCSEL_I2C);
        out_w(GPIO(I2C_SCL_1), GPIO_IE | GPIO_DRIVE_4MA | GPIO_PUE | GPIO_SHEMITT);
        break;
    }

    out_w(I2C_ENABLE(unit), 0);     // I2Cモジュール無効
}

/*----------------------------------------------------------------------
 * デバイスオープン関数
 */
ER dev_i2c_open(UW unit, UINT omode)
{
    T_CSEM  csem = {
        .isemcnt    = 1,
        .maxsem     = 1,
    };
    ER  err;

    if(unit >1) return E_PAR;
    if(ll_devcb[unit].init == FALSE) {
        err = tk_cre_sem(&csem);       // 排他制御用のセマフォ生成
        if(err < E_OK) return err;

        i2c_init(unit);
        ll_devcb[unit].semid = (ID)err;
        ll_devcb[unit].omode = omode;
        ll_devcb[unit].init = TRUE;

        out_w(I2C_ENABLE(unit), 0);         // I2Cモジュール無効

        out_w(I2C_CON(unit), I2C_CON_SLAVE_DISABLE | I2C_CON_MASTER_MODE    // Master Mode設定
            | I2C_CON_RESTART_EN | I2C_CON_TX_EMPTY_CTRL
            | I2C_CON_7BIT_ADR | I2C_CON_SPEED_FAST);                       // 7bit Address & Fast Speed設定

        out_w(I2C_RX_TL(unit), 0);
        out_w(I2C_TX_TL(unit), 0);
        out_w(I2C_DMA_CR(unit), I2C_DMA_CR_RDMAE | I2C_DMA_CR_TDMAE);

        /* 速度設定 (400Kbts)*/
        out_w(I2C_FS_SCL_HCNT(unit), 126);
        out_w(I2C_FS_SCL_LCNT(unit), 187);
        out_w(I2C_FS_SPKLEN(unit), 187/16);
        out_w(I2C_SDA_HOLD(unit), 38);
    }
    return E_OK;
}

/*----------------------------------------------------------------------
 * I2C通信実行
 */
static ER i2c_trans(INT unit)
{
    T_I2C_LLDCB *p_cb;
    UW          cmd, sts;
    UINT        state = I2C_STS_START;
    UW          abort;
    ER          err = E_OK;

    p_cb            = &ll_devcb[unit];

    out_w(I2C_TAR(unit), p_cb->sadr & I2C_TAR_7BIT_ADR);    // スレーブアドレス設定
    out_w(I2C_ENABLE(unit), I2C_ENABLE_ENABLE);             // I2Cモジュール有効

    if(p_cb->sdat_num) {            // 送信データあり
        state = I2C_STS_SEND;       // -> SEND状態に移行
    } else if(p_cb->rdat_num) {     // 受信希望あり
        state = I2C_STS_RECV;       // -> RECV状態に移行
    } else {
        err = E_PAR;
        state = I2C_STS_STOP;
    }

    out_w(I2C_CLR_INTR(unit), 0);   // 割り込みレジスタを全クリア
    in_w(I2C_CLR_TX_ABRT(unit));    // TX_ABRTレジスタをクリア

    while(state != I2C_STS_STOP) {  // 停止状態になるまで実行
        switch(state){
        case I2C_STS_SEND:          // 送信の場合
            cmd = *(p_cb->sbuf)++ & I2C_DATA_CMD_DAT;   // 送信バッファから1バイト取り出してUWのcmdにいれる
            if(--(p_cb->sdat_num)) {    // まだ送信データがある場合
                cmd |= I2C_DATA_CMD_CMD_WRITE;  // 送信継続
            } else {    // 最後の送信データの場合
                if(p_cb->rdat_num) {    // 受信バッファが指定されている場合
                    // このコマンド前にRESTARTを発行
                    cmd |= I2C_DATA_CMD_CMD_WRITE | I2C_DATA_CMD_RESTART;
                    state = I2C_STS_RECV;   // 受信状態へ
                } else {
                    // このコマンド後にSTOPを発行
                    cmd |= I2C_DATA_CMD_CMD_WRITE | I2C_DATA_CMD_STOP;
                    state = I2C_STS_STOP;   // STOP状態へ
                }
            }
            out_w(I2C_DATA_CMD(unit), cmd); // 1バイト送信
            // 1バイト送信の終了を待つ
            while(1) {
                sts = in_w(I2C_RAW_INTR_STAT(unit));
                if(sts & I2C_RAW_INTR_STAT_TX_EMPTY) break;
            }

            break;
        case I2C_STS_RECV:          // 受信の場合
            if(--(p_cb->rdat_num)) {    // まだ受信データがある場合
                cmd = I2C_DATA_CMD_CMD_READ;    // 受信継続
            } else {    // Last data    // 最後のデータの場合
                cmd = I2C_DATA_CMD_CMD_READ | I2C_DATA_CMD_STOP;
                state = I2C_STS_STOP;   // STOP状態へ
            }
            out_w(I2C_DATA_CMD(unit), cmd);
            // 1バイト受信の終了を待つ
            while(1) {
                abort = in_w(I2C_TX_ABRT_SOURCE(unit));
                if(abort != 0) {
                    err = E_IO;
                    state = I2C_STS_STOP;
                    break;
                }
                sts = in_w(I2C_RXFLR(unit));    // 受信FIFO内のデータ数
                if(sts != 0) break;
            }
            // 受信データを受信バッファに入れる
            *(p_cb->rbuf)++ = in_w(I2C_DATA_CMD(unit)) & I2C_DATA_CMD_DAT;
            break;
        default:
            break;
        }
    }
    out_w(I2C_ENABLE(unit), 0);     // I2Cモジュール無効

    return err;
}

/*----------------------------------------------------------------------
 * 通信パラメータ設定
 */
static void set_com_start(UW unit, UW sadr, UW sdat_num, UW rdat_num, UB *sbuf, UB *rbuf)
{
    ll_devcb[unit].sadr     = sadr;         /* スレーブアドレス */
    ll_devcb[unit].sdat_num = sdat_num;     /* 送信データ数 */
    ll_devcb[unit].rdat_num = rdat_num;     /* 受信データ数 */
    ll_devcb[unit].sbuf     = sbuf;         /* 送信バッファ */
    ll_devcb[unit].rbuf     = rbuf;         /* 受信バッファ */
}

/*----------------------------------------------------------------------
 * デバイスリード関数
 */
ER dev_i2c_read(UW unit, UW sadr, UB *buf, SZ size, SZ *asize)
{
    ER    err;

    if(unit >1) return E_PAR;

    err = tk_wai_sem( ll_devcb[unit].semid, 1, TMO_FEVR);
    if(err < E_OK) return err;

    set_com_start( unit, sadr, 0, size, NULL, buf);
    err = i2c_trans(unit);
    if(asize != NULL) *asize = (err == E_OK)?size:0;

    tk_sig_sem( ll_devcb[unit].semid, 1);
    return err;
}

/*----------------------------------------------------------------------
 * デバイスライト関数
 */
ER dev_i2c_write(UW unit, UW sadr, UB *buf, SZ size, SZ *asize)
{
    T_I2C_EXEC  *p_ex;
    ER          err;

    if(unit >1) return E_PAR;

    err = tk_wai_sem( ll_devcb[unit].semid, 1, TMO_FEVR);
    if(err < E_OK) return err;

    if(sadr == TDN_I2C_EXEC) {
        p_ex = (T_I2C_EXEC*)buf;
        // 指定レジスタの値を取得: p_ex->rbuf
        set_com_start( unit, p_ex->sadr, 1, size, p_ex->sbuf, p_ex->rbuf);
    } else {
        // デバイスにbufからsizeだけ書き込む: 受信データは破棄
        set_com_start( unit, sadr, size, 0, buf, NULL);
    }

    err = i2c_trans(unit);
    if(asize != NULL) *asize = (err == E_OK)?size:0;

    tk_sig_sem( ll_devcb[unit].semid, 1);
    return err;
}
