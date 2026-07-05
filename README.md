# kdnapper
kdnapper is a Proof of Concept (PoC) developed to demonstrate that kdmapper, and similar vulnerable driver mappers, leaves highly detectable traces in user-mode. While this specific detection method might not warrant an immediate, automated ban due to the rare possibility of false positives or the user being infected with unrelated malware, it serves as a powerful heuristic to heavily flag suspicious accounts for further review. Because public detection vectors are inevitably bypassed once published, I have opted to release only one specific detection method out of several I initially planned. Ultimately, this is a simple behavioral catch that developers can easily adapt, or simply consider a meme.

# Why did I make this?
The concept for this tool originated during a discussion about kdmapper around 8 months ago. I finally decided to put the theory into practice, and the results have proven highly effective, as you can see in the attached video demonstration. At its core, kdmapper is a vulnerable driver abuser. To function, it must drop and load a vulnerable payload onto the disk before cleaning it up. By analyzing file system activity for these rapid drop-load-delete cycles, we can detect the exact behavioral footprint of the mapper. This PoC specifically targets the current, public GitHub release of kdmapper. It relies heavily on statistical file-system heuristics, such as calculating Shannon entropy and enforcing a ~35%-65% uppercase character ratio, to identify the pseudo-random string generator used for the dropped driver. I have tightened these behavioral thresholds as much as possible to eliminate false positives without missing actual detections.

# Notes

> This component operates entirely in user-mode using documented Windows APIs. However, because it interacts with the NTFS USN Journal, it requires Administrator privileges.

> This is not production-ready, you'd preferably want to fix up the detections slightly and also scan all disks rather than solely C:/.
