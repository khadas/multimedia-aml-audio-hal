base on platform/hardware/amlogic, r-tv-dev.

commit 16a4c906d792bd770dbf373d0a523c9a81eba8a7 (HEAD -> r-tv-dev, origin/r-tv-dev)
Author: yujie.wu <yujie.wu@amlogic.com>
Date:   Wed Jun 23 10:23:21 2021 +0800

    audio: Fix CTS Audio Capabilities failed issue  [2/2]
    
    PD#SWPL-52121
    
    Problem:
      When test CTS Audio Capabilities, it is failed.
    
    Solution:
      When atmos enabled, the APK will check the 6ch pcm cap,
      So we need add 6ch pcm for ms12 case.
    
    Verify:
      ohm
    
    Signed-off-by: yujie.wu <yujie.wu@amlogic.com>
    Change-Id: Ib48dfdd5965f69e7f322ebd339b124e147f51c8c