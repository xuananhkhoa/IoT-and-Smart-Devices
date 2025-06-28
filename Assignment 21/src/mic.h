class Mic
{
public:
    Mic()
    {
        _isRecording = false;
        _isRecordingReady = false;
    }

    void dmaHandler()
    {
        static uint8_t count = 0;

        if (DMAC->Channel[1].CHINTFLAG.bit.SUSP)
        {
            DMAC->Channel[1].CHCTRLB.reg = DMAC_CHCTRLB_CMD_RESUME;
            DMAC->Channel[1].CHINTFLAG.bit.SUSP = 1;

            if (count)
            {
                audioCallback(_adc_buf_0, ADC_BUF_LEN);
            }
            else
            {
                audioCallback(_adc_buf_1, ADC_BUF_LEN);
            }

            count = (count + 1) % 2;
        }
    }

    void startRecording()
    {
        _isRecording = true;
        _isRecordingReady = false;
    }

    bool isRecording()
    {
        return _isRecording;
    }

    bool isRecordingReady()
    {
        return _isRecordingReady;
    }

    void init()
    {
        analogReference(AR_INTERNAL2V23);

        _writer.init();

        initBufferHeader();
        configureDmaAdc();
    }

    void reset()
    {
        _isRecordingReady = false;
        _isRecording = false;

        _writer.reset();

        initBufferHeader();
    }

private:
    volatile bool _isRecording;
    volatile bool _isRecordingReady;
    FlashWriter _writer;

    typedef struct
    {
        uint16_t btctrl;
        uint16_t btcnt;
        uint32_t srcaddr;
        uint32_t dstaddr;
        uint32_t descaddr;
    } dmacdescriptor;

    volatile dmacdescriptor _wrb[DMAC_CH_NUM] __attribute__((aligned(16)));
    dmacdescriptor _descriptor_section[DMAC_CH_NUM] __attribute__((aligned(16)));
    dmacdescriptor _descriptor __attribute__((aligned(16)));

    uint16_t _adc_buf_0[ADC_BUF_LEN];
    uint16_t _adc_buf_1[ADC_BUF_LEN];

    struct wavFileHeader
    {
        char riff[4];
        long flength;
        char wave[4];
        char fmt[4];
        long chunk_size;
        short format_tag;
        short num_chans;
        long srate;
        long bytes_per_sec;
        short bytes_per_samp;
        short bits_per_samp;
        char data[4];
        long dlength;
    };

    void audioCallback(uint16_t *buf, uint32_t buf_len)
    {
        static uint32_t idx = 44;

        if (_isRecording)
        {
            for (uint32_t i = 0; i < buf_len; i++)
            {
                int16_t audio_value = ((int16_t)buf[i] - 2048) * 16;
                _writer.writeSfudBuffer(audio_value & 0xFF);
                _writer.writeSfudBuffer((audio_value >> 8) & 0xFF);
            }

            idx += buf_len;

            if (idx >= BUFFER_SIZE)
            {
                _writer.flushSfudBuffer();
                idx = 44;
                _isRecording = false;
                _isRecordingReady = true;
            }
        }
    }

    void initBufferHeader()
    {
        wavFileHeader wavh;

        strncpy(wavh.riff, "RIFF", 4);
        strncpy(wavh.wave, "WAVE", 4);
        strncpy(wavh.fmt, "fmt ", 4);
        strncpy(wavh.data, "data", 4);

        wavh.chunk_size = 16;
        wavh.format_tag = 1;
        wavh.num_chans = 1;
        wavh.srate = RATE;
        wavh.bytes_per_sec = (RATE * 1 * 16 * 1) / 8;
        wavh.bytes_per_samp = 2;
        wavh.bits_per_samp = 16;
        wavh.dlength = RATE * 2 * 1 * 16 / 2;
        wavh.flength = wavh.dlength + 44;

        _writer.writeSfudBuffer((byte *)&wavh, 44);
    }

    void configureDmaAdc()
    {
        DMAC->BASEADDR.reg = (uint32_t)_descriptor_section;
        DMAC->WRBADDR.reg = (uint32_t)_wrb;
        DMAC->CTRL.reg = DMAC_CTRL_DMAENABLE | DMAC_CTRL_LVLEN(0xf);

        DMAC->Channel[1].CHCTRLA.reg = DMAC_CHCTRLA_TRIGSRC(TC5_DMAC_ID_OVF) |
                                       DMAC_CHCTRLA_TRIGACT_BURST;

        // Descriptor for buffer 0
        _descriptor.descaddr = (uint32_t)&_descriptor_section[1];
        _descriptor.srcaddr = (uint32_t)&ADC1->RESULT.reg;
        _descriptor.dstaddr = (uint32_t)_adc_buf_0 + sizeof(uint16_t) * ADC_BUF_LEN;
        _descriptor.btcnt = ADC_BUF_LEN;
        _descriptor.btctrl = DMAC_BTCTRL_BEATSIZE_HWORD |
                              DMAC_BTCTRL_DSTINC |
                              DMAC_BTCTRL_VALID |
                              DMAC_BTCTRL_BLOCKACT_SUSPEND;
        memcpy(&_descriptor_section[0], &_descriptor, sizeof(_descriptor));

        // Descriptor for buffer 1
        _descriptor.descaddr = (uint32_t)&_descriptor_section[0];
        _descriptor.srcaddr = (uint32_t)&ADC1->RESULT.reg;
        _descriptor.dstaddr = (uint32_t)_adc_buf_1 + sizeof(uint16_t) * ADC_BUF_LEN;
        _descriptor.btcnt = ADC_BUF_LEN;
        _descriptor.btctrl = DMAC_BTCTRL_BEATSIZE_HWORD |
                              DMAC_BTCTRL_DSTINC |
                              DMAC_BTCTRL_VALID |
                              DMAC_BTCTRL_BLOCKACT_SUSPEND;
        memcpy(&_descriptor_section[1], &_descriptor, sizeof(_descriptor));

        NVIC_SetPriority(DMAC_1_IRQn, 0);
        NVIC_EnableIRQ(DMAC_1_IRQn);
        DMAC->Channel[1].CHINTENSET.reg = DMAC_CHINTENSET_SUSP;

        ADC1->INPUTCTRL.bit.MUXPOS = ADC_INPUTCTRL_MUXPOS_AIN12_Val;
        while (ADC1->SYNCBUSY.bit.INPUTCTRL)
            ;
        ADC1->SAMPCTRL.bit.SAMPLEN = 0x00;
        while (ADC1->SYNCBUSY.bit.SAMPCTRL)
            ;
        ADC1->CTRLA.reg = ADC_CTRLA_PRESCALER_DIV128;
        ADC1->CTRLB.reg = ADC_CTRLB_RESSEL_12BIT | ADC_CTRLB_FREERUN;
        while (ADC1->SYNCBUSY.bit.CTRLB)
            ;
        ADC1->CTRLA.bit.ENABLE = 1;
        while (ADC1->SYNCBUSY.bit.ENABLE)
            ;
        ADC1->SWTRIG.bit.START = 1;
        while (ADC1->SYNCBUSY.bit.SWTRIG)
            ;

        DMAC->Channel[1].CHCTRLA.bit.ENABLE = 1;

        GCLK->PCHCTRL[TC5_GCLK_ID].reg = GCLK_PCHCTRL_CHEN | GCLK_PCHCTRL_GEN_GCLK1;
        TC5->COUNT16.WAVE.reg = TC_WAVE_WAVEGEN_MFRQ;
        TC5->COUNT16.CC[0].reg = 3000 - 1;
        while (TC5->COUNT16.SYNCBUSY.bit.CC0)
            ;
        TC5->COUNT16.CTRLA.bit.ENABLE = 1;
        while (TC5->COUNT16.SYNCBUSY.bit.ENABLE)
            ;
    }
};
