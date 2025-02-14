static char rcsid[] = "$Id: 4873214ff71f3fcd58002d071f6d30d6a3d4db5f $";
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "repetitive.h"

#define NREPETITIVE 76		/* same for 6 through 15 */
static Oligospace_T repetitive_oligos_6[NREPETITIVE] =
  {0, /* AAAAAA */
   65, /* AACAAC */
   130, /* AAGAAG */
   195, /* AATAAT */
   260, /* ACAACA */
   273, /* ACACAC */
   325, /* ACCACC */
   390, /* ACGACG */
   455, /* ACTACT */
   520, /* AGAAGA */
   546, /* AGAGAG */
   585, /* AGCAGC */
   650, /* AGGAGG */
   715, /* AGTAGT */
   780, /* ATAATA */
   819, /* ATATAT */
   845, /* ATCATC */
   910, /* ATGATG */
   975, /* ATTATT */
   1040, /* CAACAA */
   1092, /* CACACA */
   1105, /* CACCAC */
   1170, /* CAGCAG */
   1235, /* CATCAT */
   1300, /* CCACCA */
   1365, /* CCCCCC */
   1430, /* CCGCCG */
   1495, /* CCTCCT */
   1560, /* CGACGA */
   1625, /* CGCCGC */
   1638, /* CGCGCG */
   1690, /* CGGCGG */
   1755, /* CGTCGT */
   1820, /* CTACTA */
   1885, /* CTCCTC */
   1911, /* CTCTCT */
   1950, /* CTGCTG */
   2015, /* CTTCTT */
   2080, /* GAAGAA */
   2145, /* GACGAC */
   2184, /* GAGAGA */
   2210, /* GAGGAG */
   2275, /* GATGAT */
   2340, /* GCAGCA */
   2405, /* GCCGCC */
   2457, /* GCGCGC */
   2470, /* GCGGCG */
   2535, /* GCTGCT */
   2600, /* GGAGGA */
   2665, /* GGCGGC */
   2730, /* GGGGGG */
   2795, /* GGTGGT */
   2860, /* GTAGTA */
   2925, /* GTCGTC */
   2990, /* GTGGTG */
   3003, /* GTGTGT */
   3055, /* GTTGTT */
   3120, /* TAATAA */
   3185, /* TACTAC */
   3250, /* TAGTAG */
   3276, /* TATATA */
   3315, /* TATTAT */
   3380, /* TCATCA */
   3445, /* TCCTCC */
   3510, /* TCGTCG */
   3549, /* TCTCTC */
   3575, /* TCTTCT */
   3640, /* TGATGA */
   3705, /* TGCTGC */
   3770, /* TGGTGG */
   3822, /* TGTGTG */
   3835, /* TGTTGT */
   3900, /* TTATTA */
   3965, /* TTCTTC */
   4030, /* TTGTTG */
   4095, /* TTTTTT */
  };

static Oligospace_T repetitive_oligos_7[NREPETITIVE] =
  {0, /* AAAAAAA */
   260, /* AACAACA */
   520, /* AAGAAGA */
   780, /* AATAATA */
   1040, /* ACAACAA */
   1092, /* ACACACA */
   1300, /* ACCACCA */
   1560, /* ACGACGA */
   1820, /* ACTACTA */
   2080, /* AGAAGAA */
   2184, /* AGAGAGA */
   2340, /* AGCAGCA */
   2600, /* AGGAGGA */
   2860, /* AGTAGTA */
   3120, /* ATAATAA */
   3276, /* ATATATA */
   3380, /* ATCATCA */
   3640, /* ATGATGA */
   3900, /* ATTATTA */
   4161, /* CAACAAC */
   4369, /* CACACAC */
   4421, /* CACCACC */
   4681, /* CAGCAGC */
   4941, /* CATCATC */
   5201, /* CCACCAC */
   5461, /* CCCCCCC */
   5721, /* CCGCCGC */
   5981, /* CCTCCTC */
   6241, /* CGACGAC */
   6501, /* CGCCGCC */
   6553, /* CGCGCGC */
   6761, /* CGGCGGC */
   7021, /* CGTCGTC */
   7281, /* CTACTAC */
   7541, /* CTCCTCC */
   7645, /* CTCTCTC */
   7801, /* CTGCTGC */
   8061, /* CTTCTTC */
   8322, /* GAAGAAG */
   8582, /* GACGACG */
   8738, /* GAGAGAG */
   8842, /* GAGGAGG */
   9102, /* GATGATG */
   9362, /* GCAGCAG */
   9622, /* GCCGCCG */
   9830, /* GCGCGCG */
   9882, /* GCGGCGG */
   10142, /* GCTGCTG */
   10402, /* GGAGGAG */
   10662, /* GGCGGCG */
   10922, /* GGGGGGG */
   11182, /* GGTGGTG */
   11442, /* GTAGTAG */
   11702, /* GTCGTCG */
   11962, /* GTGGTGG */
   12014, /* GTGTGTG */
   12222, /* GTTGTTG */
   12483, /* TAATAAT */
   12743, /* TACTACT */
   13003, /* TAGTAGT */
   13107, /* TATATAT */
   13263, /* TATTATT */
   13523, /* TCATCAT */
   13783, /* TCCTCCT */
   14043, /* TCGTCGT */
   14199, /* TCTCTCT */
   14303, /* TCTTCTT */
   14563, /* TGATGAT */
   14823, /* TGCTGCT */
   15083, /* TGGTGGT */
   15291, /* TGTGTGT */
   15343, /* TGTTGTT */
   15603, /* TTATTAT */
   15863, /* TTCTTCT */
   16123, /* TTGTTGT */
   16383, /* TTTTTTT */
  };

static Oligospace_T repetitive_oligos_8[NREPETITIVE] =
  {0, /* AAAAAAAA */
   1040, /* AACAACAA */
   2080, /* AAGAAGAA */
   3120, /* AATAATAA */
   4161, /* ACAACAAC */
   4369, /* ACACACAC */
   5201, /* ACCACCAC */
   6241, /* ACGACGAC */
   7281, /* ACTACTAC */
   8322, /* AGAAGAAG */
   8738, /* AGAGAGAG */
   9362, /* AGCAGCAG */
   10402, /* AGGAGGAG */
   11442, /* AGTAGTAG */
   12483, /* ATAATAAT */
   13107, /* ATATATAT */
   13523, /* ATCATCAT */
   14563, /* ATGATGAT */
   15603, /* ATTATTAT */
   16644, /* CAACAACA */
   17476, /* CACACACA */
   17684, /* CACCACCA */
   18724, /* CAGCAGCA */
   19764, /* CATCATCA */
   20805, /* CCACCACC */
   21845, /* CCCCCCCC */
   22885, /* CCGCCGCC */
   23925, /* CCTCCTCC */
   24966, /* CGACGACG */
   26006, /* CGCCGCCG */
   26214, /* CGCGCGCG */
   27046, /* CGGCGGCG */
   28086, /* CGTCGTCG */
   29127, /* CTACTACT */
   30167, /* CTCCTCCT */
   30583, /* CTCTCTCT */
   31207, /* CTGCTGCT */
   32247, /* CTTCTTCT */
   33288, /* GAAGAAGA */
   34328, /* GACGACGA */
   34952, /* GAGAGAGA */
   35368, /* GAGGAGGA */
   36408, /* GATGATGA */
   37449, /* GCAGCAGC */
   38489, /* GCCGCCGC */
   39321, /* GCGCGCGC */
   39529, /* GCGGCGGC */
   40569, /* GCTGCTGC */
   41610, /* GGAGGAGG */
   42650, /* GGCGGCGG */
   43690, /* GGGGGGGG */
   44730, /* GGTGGTGG */
   45771, /* GTAGTAGT */
   46811, /* GTCGTCGT */
   47851, /* GTGGTGGT */
   48059, /* GTGTGTGT */
   48891, /* GTTGTTGT */
   49932, /* TAATAATA */
   50972, /* TACTACTA */
   52012, /* TAGTAGTA */
   52428, /* TATATATA */
   53052, /* TATTATTA */
   54093, /* TCATCATC */
   55133, /* TCCTCCTC */
   56173, /* TCGTCGTC */
   56797, /* TCTCTCTC */
   57213, /* TCTTCTTC */
   58254, /* TGATGATG */
   59294, /* TGCTGCTG */
   60334, /* TGGTGGTG */
   61166, /* TGTGTGTG */
   61374, /* TGTTGTTG */
   62415, /* TTATTATT */
   63455, /* TTCTTCTT */
   64495, /* TTGTTGTT */
   65535, /* TTTTTTTT */
  };

static Oligospace_T repetitive_oligos_9[NREPETITIVE] =
  {0, /* AAAAAAAAA */
   4161, /* AACAACAAC */
   8322, /* AAGAAGAAG */
   12483, /* AATAATAAT */
   16644, /* ACAACAACA */
   17476, /* ACACACACA */
   20805, /* ACCACCACC */
   24966, /* ACGACGACG */
   29127, /* ACTACTACT */
   33288, /* AGAAGAAGA */
   34952, /* AGAGAGAGA */
   37449, /* AGCAGCAGC */
   41610, /* AGGAGGAGG */
   45771, /* AGTAGTAGT */
   49932, /* ATAATAATA */
   52428, /* ATATATATA */
   54093, /* ATCATCATC */
   58254, /* ATGATGATG */
   62415, /* ATTATTATT */
   66576, /* CAACAACAA */
   69905, /* CACACACAC */
   70737, /* CACCACCAC */
   74898, /* CAGCAGCAG */
   79059, /* CATCATCAT */
   83220, /* CCACCACCA */
   87381, /* CCCCCCCCC */
   91542, /* CCGCCGCCG */
   95703, /* CCTCCTCCT */
   99864, /* CGACGACGA */
   104025, /* CGCCGCCGC */
   104857, /* CGCGCGCGC */
   108186, /* CGGCGGCGG */
   112347, /* CGTCGTCGT */
   116508, /* CTACTACTA */
   120669, /* CTCCTCCTC */
   122333, /* CTCTCTCTC */
   124830, /* CTGCTGCTG */
   128991, /* CTTCTTCTT */
   133152, /* GAAGAAGAA */
   137313, /* GACGACGAC */
   139810, /* GAGAGAGAG */
   141474, /* GAGGAGGAG */
   145635, /* GATGATGAT */
   149796, /* GCAGCAGCA */
   153957, /* GCCGCCGCC */
   157286, /* GCGCGCGCG */
   158118, /* GCGGCGGCG */
   162279, /* GCTGCTGCT */
   166440, /* GGAGGAGGA */
   170601, /* GGCGGCGGC */
   174762, /* GGGGGGGGG */
   178923, /* GGTGGTGGT */
   183084, /* GTAGTAGTA */
   187245, /* GTCGTCGTC */
   191406, /* GTGGTGGTG */
   192238, /* GTGTGTGTG */
   195567, /* GTTGTTGTT */
   199728, /* TAATAATAA */
   203889, /* TACTACTAC */
   208050, /* TAGTAGTAG */
   209715, /* TATATATAT */
   212211, /* TATTATTAT */
   216372, /* TCATCATCA */
   220533, /* TCCTCCTCC */
   224694, /* TCGTCGTCG */
   227191, /* TCTCTCTCT */
   228855, /* TCTTCTTCT */
   233016, /* TGATGATGA */
   237177, /* TGCTGCTGC */
   241338, /* TGGTGGTGG */
   244667, /* TGTGTGTGT */
   245499, /* TGTTGTTGT */
   249660, /* TTATTATTA */
   253821, /* TTCTTCTTC */
   257982, /* TTGTTGTTG */
   262143, /* TTTTTTTTT */
  };

static Oligospace_T repetitive_oligos_10[NREPETITIVE] =
  {0, /* AAAAAAAAAA */
   16644, /* AACAACAACA */
   33288, /* AAGAAGAAGA */
   49932, /* AATAATAATA */
   66576, /* ACAACAACAA */
   69905, /* ACACACACAC */
   83220, /* ACCACCACCA */
   99864, /* ACGACGACGA */
   116508, /* ACTACTACTA */
   133152, /* AGAAGAAGAA */
   139810, /* AGAGAGAGAG */
   149796, /* AGCAGCAGCA */
   166440, /* AGGAGGAGGA */
   183084, /* AGTAGTAGTA */
   199728, /* ATAATAATAA */
   209715, /* ATATATATAT */
   216372, /* ATCATCATCA */
   233016, /* ATGATGATGA */
   249660, /* ATTATTATTA */
   266305, /* CAACAACAAC */
   279620, /* CACACACACA */
   282949, /* CACCACCACC */
   299593, /* CAGCAGCAGC */
   316237, /* CATCATCATC */
   332881, /* CCACCACCAC */
   349525, /* CCCCCCCCCC */
   366169, /* CCGCCGCCGC */
   382813, /* CCTCCTCCTC */
   399457, /* CGACGACGAC */
   416101, /* CGCCGCCGCC */
   419430, /* CGCGCGCGCG */
   432745, /* CGGCGGCGGC */
   449389, /* CGTCGTCGTC */
   466033, /* CTACTACTAC */
   482677, /* CTCCTCCTCC */
   489335, /* CTCTCTCTCT */
   499321, /* CTGCTGCTGC */
   515965, /* CTTCTTCTTC */
   532610, /* GAAGAAGAAG */
   549254, /* GACGACGACG */
   559240, /* GAGAGAGAGA */
   565898, /* GAGGAGGAGG */
   582542, /* GATGATGATG */
   599186, /* GCAGCAGCAG */
   615830, /* GCCGCCGCCG */
   629145, /* GCGCGCGCGC */
   632474, /* GCGGCGGCGG */
   649118, /* GCTGCTGCTG */
   665762, /* GGAGGAGGAG */
   682406, /* GGCGGCGGCG */
   699050, /* GGGGGGGGGG */
   715694, /* GGTGGTGGTG */
   732338, /* GTAGTAGTAG */
   748982, /* GTCGTCGTCG */
   765626, /* GTGGTGGTGG */
   768955, /* GTGTGTGTGT */
   782270, /* GTTGTTGTTG */
   798915, /* TAATAATAAT */
   815559, /* TACTACTACT */
   832203, /* TAGTAGTAGT */
   838860, /* TATATATATA */
   848847, /* TATTATTATT */
   865491, /* TCATCATCAT */
   882135, /* TCCTCCTCCT */
   898779, /* TCGTCGTCGT */
   908765, /* TCTCTCTCTC */
   915423, /* TCTTCTTCTT */
   932067, /* TGATGATGAT */
   948711, /* TGCTGCTGCT */
   965355, /* TGGTGGTGGT */
   978670, /* TGTGTGTGTG */
   981999, /* TGTTGTTGTT */
   998643, /* TTATTATTAT */
   1015287, /* TTCTTCTTCT */
   1031931, /* TTGTTGTTGT */
   1048575, /* TTTTTTTTTT */
  };

static Oligospace_T repetitive_oligos_11[NREPETITIVE] =
  {0, /* AAAAAAAAAAA */
   66576, /* AACAACAACAA */
   133152, /* AAGAAGAAGAA */
   199728, /* AATAATAATAA */
   266305, /* ACAACAACAAC */
   279620, /* ACACACACACA */
   332881, /* ACCACCACCAC */
   399457, /* ACGACGACGAC */
   466033, /* ACTACTACTAC */
   532610, /* AGAAGAAGAAG */
   559240, /* AGAGAGAGAGA */
   599186, /* AGCAGCAGCAG */
   665762, /* AGGAGGAGGAG */
   732338, /* AGTAGTAGTAG */
   798915, /* ATAATAATAAT */
   838860, /* ATATATATATA */
   865491, /* ATCATCATCAT */
   932067, /* ATGATGATGAT */
   998643, /* ATTATTATTAT */
   1065220, /* CAACAACAACA */
   1118481, /* CACACACACAC */
   1131796, /* CACCACCACCA */
   1198372, /* CAGCAGCAGCA */
   1264948, /* CATCATCATCA */
   1331525, /* CCACCACCACC */
   1398101, /* CCCCCCCCCCC */
   1464677, /* CCGCCGCCGCC */
   1531253, /* CCTCCTCCTCC */
   1597830, /* CGACGACGACG */
   1664406, /* CGCCGCCGCCG */
   1677721, /* CGCGCGCGCGC */
   1730982, /* CGGCGGCGGCG */
   1797558, /* CGTCGTCGTCG */
   1864135, /* CTACTACTACT */
   1930711, /* CTCCTCCTCCT */
   1957341, /* CTCTCTCTCTC */
   1997287, /* CTGCTGCTGCT */
   2063863, /* CTTCTTCTTCT */
   2130440, /* GAAGAAGAAGA */
   2197016, /* GACGACGACGA */
   2236962, /* GAGAGAGAGAG */
   2263592, /* GAGGAGGAGGA */
   2330168, /* GATGATGATGA */
   2396745, /* GCAGCAGCAGC */
   2463321, /* GCCGCCGCCGC */
   2516582, /* GCGCGCGCGCG */
   2529897, /* GCGGCGGCGGC */
   2596473, /* GCTGCTGCTGC */
   2663050, /* GGAGGAGGAGG */
   2729626, /* GGCGGCGGCGG */
   2796202, /* GGGGGGGGGGG */
   2862778, /* GGTGGTGGTGG */
   2929355, /* GTAGTAGTAGT */
   2995931, /* GTCGTCGTCGT */
   3062507, /* GTGGTGGTGGT */
   3075822, /* GTGTGTGTGTG */
   3129083, /* GTTGTTGTTGT */
   3195660, /* TAATAATAATA */
   3262236, /* TACTACTACTA */
   3328812, /* TAGTAGTAGTA */
   3355443, /* TATATATATAT */
   3395388, /* TATTATTATTA */
   3461965, /* TCATCATCATC */
   3528541, /* TCCTCCTCCTC */
   3595117, /* TCGTCGTCGTC */
   3635063, /* TCTCTCTCTCT */
   3661693, /* TCTTCTTCTTC */
   3728270, /* TGATGATGATG */
   3794846, /* TGCTGCTGCTG */
   3861422, /* TGGTGGTGGTG */
   3914683, /* TGTGTGTGTGT */
   3927998, /* TGTTGTTGTTG */
   3994575, /* TTATTATTATT */
   4061151, /* TTCTTCTTCTT */
   4127727, /* TTGTTGTTGTT */
   4194303, /* TTTTTTTTTTT */
  };


static Oligospace_T repetitive_oligos_12[NREPETITIVE] =
  {0, /* AAAAAAAAAAAA */
   266305, /* AACAACAACAAC */
   532610, /* AAGAAGAAGAAG */
   798915, /* AATAATAATAAT */
   1065220, /* ACAACAACAACA */
   1118481, /* ACACACACACAC */
   1331525, /* ACCACCACCACC */
   1597830, /* ACGACGACGACG */
   1864135, /* ACTACTACTACT */
   2130440, /* AGAAGAAGAAGA */
   2236962, /* AGAGAGAGAGAG */
   2396745, /* AGCAGCAGCAGC */
   2663050, /* AGGAGGAGGAGG */
   2929355, /* AGTAGTAGTAGT */
   3195660, /* ATAATAATAATA */
   3355443, /* ATATATATATAT */
   3461965, /* ATCATCATCATC */
   3728270, /* ATGATGATGATG */
   3994575, /* ATTATTATTATT */
   4260880, /* CAACAACAACAA */
   4473924, /* CACACACACACA */
   4527185, /* CACCACCACCAC */
   4793490, /* CAGCAGCAGCAG */
   5059795, /* CATCATCATCAT */
   5326100, /* CCACCACCACCA */
   5592405, /* CCCCCCCCCCCC */
   5858710, /* CCGCCGCCGCCG */
   6125015, /* CCTCCTCCTCCT */
   6391320, /* CGACGACGACGA */
   6657625, /* CGCCGCCGCCGC */
   6710886, /* CGCGCGCGCGCG */
   6923930, /* CGGCGGCGGCGG */
   7190235, /* CGTCGTCGTCGT */
   7456540, /* CTACTACTACTA */
   7722845, /* CTCCTCCTCCTC */
   7829367, /* CTCTCTCTCTCT */
   7989150, /* CTGCTGCTGCTG */
   8255455, /* CTTCTTCTTCTT */
   8521760, /* GAAGAAGAAGAA */
   8788065, /* GACGACGACGAC */
   8947848, /* GAGAGAGAGAGA */
   9054370, /* GAGGAGGAGGAG */
   9320675, /* GATGATGATGAT */
   9586980, /* GCAGCAGCAGCA */
   9853285, /* GCCGCCGCCGCC */
   10066329, /* GCGCGCGCGCGC */
   10119590, /* GCGGCGGCGGCG */
   10385895, /* GCTGCTGCTGCT */
   10652200, /* GGAGGAGGAGGA */
   10918505, /* GGCGGCGGCGGC */
   11184810, /* GGGGGGGGGGGG */
   11451115, /* GGTGGTGGTGGT */
   11717420, /* GTAGTAGTAGTA */
   11983725, /* GTCGTCGTCGTC */
   12250030, /* GTGGTGGTGGTG */
   12303291, /* GTGTGTGTGTGT */
   12516335, /* GTTGTTGTTGTT */
   12782640, /* TAATAATAATAA */
   13048945, /* TACTACTACTAC */
   13315250, /* TAGTAGTAGTAG */
   13421772, /* TATATATATATA */
   13581555, /* TATTATTATTAT */
   13847860, /* TCATCATCATCA */
   14114165, /* TCCTCCTCCTCC */
   14380470, /* TCGTCGTCGTCG */
   14540253, /* TCTCTCTCTCTC */
   14646775, /* TCTTCTTCTTCT */
   14913080, /* TGATGATGATGA */
   15179385, /* TGCTGCTGCTGC */
   15445690, /* TGGTGGTGGTGG */
   15658734, /* TGTGTGTGTGTG */
   15711995, /* TGTTGTTGTTGT */
   15978300, /* TTATTATTATTA */
   16244605, /* TTCTTCTTCTTC */
   16510910, /* TTGTTGTTGTTG */
   16777215, /* TTTTTTTTTTTT */
  };

static Oligospace_T repetitive_oligos_13[NREPETITIVE] =
  {0, /* AAAAAAAAAAAAA */
   1065220, /* AACAACAACAACA */
   2130440, /* AAGAAGAAGAAGA */
   3195660, /* AATAATAATAATA */
   4260880, /* ACAACAACAACAA */
   4473924, /* ACACACACACACA */
   5326100, /* ACCACCACCACCA */
   6391320, /* ACGACGACGACGA */
   7456540, /* ACTACTACTACTA */
   8521760, /* AGAAGAAGAAGAA */
   8947848, /* AGAGAGAGAGAGA */
   9586980, /* AGCAGCAGCAGCA */
   10652200, /* AGGAGGAGGAGGA */
   11717420, /* AGTAGTAGTAGTA */
   12782640, /* ATAATAATAATAA */
   13421772, /* ATATATATATATA */
   13847860, /* ATCATCATCATCA */
   14913080, /* ATGATGATGATGA */
   15978300, /* ATTATTATTATTA */
   17043521, /* CAACAACAACAAC */
   17895697, /* CACACACACACAC */
   18108741, /* CACCACCACCACC */
   19173961, /* CAGCAGCAGCAGC */
   20239181, /* CATCATCATCATC */
   21304401, /* CCACCACCACCAC */
   22369621, /* CCCCCCCCCCCCC */
   23434841, /* CCGCCGCCGCCGC */
   24500061, /* CCTCCTCCTCCTC */
   25565281, /* CGACGACGACGAC */
   26630501, /* CGCCGCCGCCGCC */
   26843545, /* CGCGCGCGCGCGC */
   27695721, /* CGGCGGCGGCGGC */
   28760941, /* CGTCGTCGTCGTC */
   29826161, /* CTACTACTACTAC */
   30891381, /* CTCCTCCTCCTCC */
   31317469, /* CTCTCTCTCTCTC */
   31956601, /* CTGCTGCTGCTGC */
   33021821, /* CTTCTTCTTCTTC */
   34087042, /* GAAGAAGAAGAAG */
   35152262, /* GACGACGACGACG */
   35791394, /* GAGAGAGAGAGAG */
   36217482, /* GAGGAGGAGGAGG */
   37282702, /* GATGATGATGATG */
   38347922, /* GCAGCAGCAGCAG */
   39413142, /* GCCGCCGCCGCCG */
   40265318, /* GCGCGCGCGCGCG */
   40478362, /* GCGGCGGCGGCGG */
   41543582, /* GCTGCTGCTGCTG */
   42608802, /* GGAGGAGGAGGAG */
   43674022, /* GGCGGCGGCGGCG */
   44739242, /* GGGGGGGGGGGGG */
   45804462, /* GGTGGTGGTGGTG */
   46869682, /* GTAGTAGTAGTAG */
   47934902, /* GTCGTCGTCGTCG */
   49000122, /* GTGGTGGTGGTGG */
   49213166, /* GTGTGTGTGTGTG */
   50065342, /* GTTGTTGTTGTTG */
   51130563, /* TAATAATAATAAT */
   52195783, /* TACTACTACTACT */
   53261003, /* TAGTAGTAGTAGT */
   53687091, /* TATATATATATAT */
   54326223, /* TATTATTATTATT */
   55391443, /* TCATCATCATCAT */
   56456663, /* TCCTCCTCCTCCT */
   57521883, /* TCGTCGTCGTCGT */
   58161015, /* TCTCTCTCTCTCT */
   58587103, /* TCTTCTTCTTCTT */
   59652323, /* TGATGATGATGAT */
   60717543, /* TGCTGCTGCTGCT */
   61782763, /* TGGTGGTGGTGGT */
   62634939, /* TGTGTGTGTGTGT */
   62847983, /* TGTTGTTGTTGTT */
   63913203, /* TTATTATTATTAT */
   64978423, /* TTCTTCTTCTTCT */
   66043643, /* TTGTTGTTGTTGT */
   67108863, /* TTTTTTTTTTTTT */
  };


static Oligospace_T repetitive_oligos_14[NREPETITIVE] =
  {0, /* AAAAAAAAAAAAAA */
   4260880, /* AACAACAACAACAA */
   8521760, /* AAGAAGAAGAAGAA */
   12782640, /* AATAATAATAATAA */
   17043521, /* ACAACAACAACAAC */
   17895697, /* ACACACACACACAC */
   21304401, /* ACCACCACCACCAC */
   25565281, /* ACGACGACGACGAC */
   29826161, /* ACTACTACTACTAC */
   34087042, /* AGAAGAAGAAGAAG */
   35791394, /* AGAGAGAGAGAGAG */
   38347922, /* AGCAGCAGCAGCAG */
   42608802, /* AGGAGGAGGAGGAG */
   46869682, /* AGTAGTAGTAGTAG */
   51130563, /* ATAATAATAATAAT */
   53687091, /* ATATATATATATAT */
   55391443, /* ATCATCATCATCAT */
   59652323, /* ATGATGATGATGAT */
   63913203, /* ATTATTATTATTAT */
   68174084, /* CAACAACAACAACA */
   71582788, /* CACACACACACACA */
   72434964, /* CACCACCACCACCA */
   76695844, /* CAGCAGCAGCAGCA */
   80956724, /* CATCATCATCATCA */
   85217605, /* CCACCACCACCACC */
   89478485, /* CCCCCCCCCCCCCC */
   93739365, /* CCGCCGCCGCCGCC */
   98000245, /* CCTCCTCCTCCTCC */
   102261126, /* CGACGACGACGACG */
   106522006, /* CGCCGCCGCCGCCG */
   107374182, /* CGCGCGCGCGCGCG */
   110782886, /* CGGCGGCGGCGGCG */
   115043766, /* CGTCGTCGTCGTCG */
   119304647, /* CTACTACTACTACT */
   123565527, /* CTCCTCCTCCTCCT */
   125269879, /* CTCTCTCTCTCTCT */
   127826407, /* CTGCTGCTGCTGCT */
   132087287, /* CTTCTTCTTCTTCT */
   136348168, /* GAAGAAGAAGAAGA */
   140609048, /* GACGACGACGACGA */
   143165576, /* GAGAGAGAGAGAGA */
   144869928, /* GAGGAGGAGGAGGA */
   149130808, /* GATGATGATGATGA */
   153391689, /* GCAGCAGCAGCAGC */
   157652569, /* GCCGCCGCCGCCGC */
   161061273, /* GCGCGCGCGCGCGC */
   161913449, /* GCGGCGGCGGCGGC */
   166174329, /* GCTGCTGCTGCTGC */
   170435210, /* GGAGGAGGAGGAGG */
   174696090, /* GGCGGCGGCGGCGG */
   178956970, /* GGGGGGGGGGGGGG */
   183217850, /* GGTGGTGGTGGTGG */
   187478731, /* GTAGTAGTAGTAGT */
   191739611, /* GTCGTCGTCGTCGT */
   196000491, /* GTGGTGGTGGTGGT */
   196852667, /* GTGTGTGTGTGTGT */
   200261371, /* GTTGTTGTTGTTGT */
   204522252, /* TAATAATAATAATA */
   208783132, /* TACTACTACTACTA */
   213044012, /* TAGTAGTAGTAGTA */
   214748364, /* TATATATATATATA */
   217304892, /* TATTATTATTATTA */
   221565773, /* TCATCATCATCATC */
   225826653, /* TCCTCCTCCTCCTC */
   230087533, /* TCGTCGTCGTCGTC */
   232644061, /* TCTCTCTCTCTCTC */
   234348413, /* TCTTCTTCTTCTTC */
   238609294, /* TGATGATGATGATG */
   242870174, /* TGCTGCTGCTGCTG */
   247131054, /* TGGTGGTGGTGGTG */
   250539758, /* TGTGTGTGTGTGTG */
   251391934, /* TGTTGTTGTTGTTG */
   255652815, /* TTATTATTATTATT */
   259913695, /* TTCTTCTTCTTCTT */
   264174575, /* TTGTTGTTGTTGTT */
   268435455, /* TTTTTTTTTTTTTT */
  };

static Oligospace_T repetitive_oligos_15[NREPETITIVE] =
  {0, /* AAAAAAAAAAAAAAA */
   17043521, /* AACAACAACAACAAC */
   34087042, /* AAGAAGAAGAAGAAG */
   51130563, /* AATAATAATAATAAT */
   68174084, /* ACAACAACAACAACA */
   71582788, /* ACACACACACACACA */
   85217605, /* ACCACCACCACCACC */
   102261126, /* ACGACGACGACGACG */
   119304647, /* ACTACTACTACTACT */
   136348168, /* AGAAGAAGAAGAAGA */
   143165576, /* AGAGAGAGAGAGAGA */
   153391689, /* AGCAGCAGCAGCAGC */
   170435210, /* AGGAGGAGGAGGAGG */
   187478731, /* AGTAGTAGTAGTAGT */
   204522252, /* ATAATAATAATAATA */
   214748364, /* ATATATATATATATA */
   221565773, /* ATCATCATCATCATC */
   238609294, /* ATGATGATGATGATG */
   255652815, /* ATTATTATTATTATT */
   272696336, /* CAACAACAACAACAA */
   286331153, /* CACACACACACACAC */
   289739857, /* CACCACCACCACCAC */
   306783378, /* CAGCAGCAGCAGCAG */
   323826899, /* CATCATCATCATCAT */
   340870420, /* CCACCACCACCACCA */
   357913941, /* CCCCCCCCCCCCCCC */
   374957462, /* CCGCCGCCGCCGCCG */
   392000983, /* CCTCCTCCTCCTCCT */
   409044504, /* CGACGACGACGACGA */
   426088025, /* CGCCGCCGCCGCCGC */
   429496729, /* CGCGCGCGCGCGCGC */
   443131546, /* CGGCGGCGGCGGCGG */
   460175067, /* CGTCGTCGTCGTCGT */
   477218588, /* CTACTACTACTACTA */
   494262109, /* CTCCTCCTCCTCCTC */
   501079517, /* CTCTCTCTCTCTCTC */
   511305630, /* CTGCTGCTGCTGCTG */
   528349151, /* CTTCTTCTTCTTCTT */
   545392672, /* GAAGAAGAAGAAGAA */
   562436193, /* GACGACGACGACGAC */
   572662306, /* GAGAGAGAGAGAGAG */
   579479714, /* GAGGAGGAGGAGGAG */
   596523235, /* GATGATGATGATGAT */
   613566756, /* GCAGCAGCAGCAGCA */
   630610277, /* GCCGCCGCCGCCGCC */
   644245094, /* GCGCGCGCGCGCGCG */
   647653798, /* GCGGCGGCGGCGGCG */
   664697319, /* GCTGCTGCTGCTGCT */
   681740840, /* GGAGGAGGAGGAGGA */
   698784361, /* GGCGGCGGCGGCGGC */
   715827882, /* GGGGGGGGGGGGGGG */
   732871403, /* GGTGGTGGTGGTGGT */
   749914924, /* GTAGTAGTAGTAGTA */
   766958445, /* GTCGTCGTCGTCGTC */
   784001966, /* GTGGTGGTGGTGGTG */
   787410670, /* GTGTGTGTGTGTGTG */
   801045487, /* GTTGTTGTTGTTGTT */
   818089008, /* TAATAATAATAATAA */
   835132529, /* TACTACTACTACTAC */
   852176050, /* TAGTAGTAGTAGTAG */
   858993459, /* TATATATATATATAT */
   869219571, /* TATTATTATTATTAT */
   886263092, /* TCATCATCATCATCA */
   903306613, /* TCCTCCTCCTCCTCC */
   920350134, /* TCGTCGTCGTCGTCG */
   930576247, /* TCTCTCTCTCTCTCT */
   937393655, /* TCTTCTTCTTCTTCT */
   954437176, /* TGATGATGATGATGA */
   971480697, /* TGCTGCTGCTGCTGC */
   988524218, /* TGGTGGTGGTGGTGG */
   1002159035, /* TGTGTGTGTGTGTGT */
   1005567739, /* TGTTGTTGTTGTTGT */
   1022611260, /* TTATTATTATTATTA */
   1039654781, /* TTCTTCTTCTTCTTC */
   1056698302, /* TTGTTGTTGTTGTTG */
   1073741823, /* TTTTTTTTTTTTTTT */
  };

static Oligospace_T repetitive_oligos_16[NREPETITIVE] =
  {0U, /* AAAAAAAAAAAAAAAA */
   68174084U, /* AACAACAACAACAACA */
   136348168U, /* AAGAAGAAGAAGAAGA */
   204522252U, /* AATAATAATAATAATA */
   272696336U, /* ACAACAACAACAACAA */
   286331153U, /* ACACACACACACACAC */
   340870420U, /* ACCACCACCACCACCA */
   409044504U, /* ACGACGACGACGACGA */
   477218588U, /* ACTACTACTACTACTA */
   545392672U, /* AGAAGAAGAAGAAGAA */
   572662306U, /* AGAGAGAGAGAGAGAG */
   613566756U, /* AGCAGCAGCAGCAGCA */
   681740840U, /* AGGAGGAGGAGGAGGA */
   749914924U, /* AGTAGTAGTAGTAGTA */
   818089008U, /* ATAATAATAATAATAA */
   858993459U, /* ATATATATATATATAT */
   886263092U, /* ATCATCATCATCATCA */
   954437176U, /* ATGATGATGATGATGA */
   1022611260U, /* ATTATTATTATTATTA */
   1090785345U, /* CAACAACAACAACAAC */
   1145324612U, /* CACACACACACACACA */
   1158959429U, /* CACCACCACCACCACC */
   1227133513U, /* CAGCAGCAGCAGCAGC */
   1295307597U, /* CATCATCATCATCATC */
   1363481681U, /* CCACCACCACCACCAC */
   1431655765U, /* CCCCCCCCCCCCCCCC */
   1499829849U, /* CCGCCGCCGCCGCCGC */
   1568003933U, /* CCTCCTCCTCCTCCTC */
   1636178017U, /* CGACGACGACGACGAC */
   1704352101U, /* CGCCGCCGCCGCCGCC */
   1717986918U, /* CGCGCGCGCGCGCGCG */
   1772526185U, /* CGGCGGCGGCGGCGGC */
   1840700269U, /* CGTCGTCGTCGTCGTC */
   1908874353U, /* CTACTACTACTACTAC */
   1977048437U, /* CTCCTCCTCCTCCTCC */
   2004318071U, /* CTCTCTCTCTCTCTCT */
   2045222521U, /* CTGCTGCTGCTGCTGC */
   2113396605U, /* CTTCTTCTTCTTCTTC */
   2181570690U, /* GAAGAAGAAGAAGAAG */
   2249744774U, /* GACGACGACGACGACG */
   2290649224U, /* GAGAGAGAGAGAGAGA */
   2317918858U, /* GAGGAGGAGGAGGAGG */
   2386092942U, /* GATGATGATGATGATG */
   2454267026U, /* GCAGCAGCAGCAGCAG */
   2522441110U, /* GCCGCCGCCGCCGCCG */
   2576980377U, /* GCGCGCGCGCGCGCGC */
   2590615194U, /* GCGGCGGCGGCGGCGG */
   2658789278U, /* GCTGCTGCTGCTGCTG */
   2726963362U, /* GGAGGAGGAGGAGGAG */
   2795137446U, /* GGCGGCGGCGGCGGCG */
   2863311530U, /* GGGGGGGGGGGGGGGG */
   2931485614U, /* GGTGGTGGTGGTGGTG */
   2999659698U, /* GTAGTAGTAGTAGTAG */
   3067833782U, /* GTCGTCGTCGTCGTCG */
   3136007866U, /* GTGGTGGTGGTGGTGG */
   3149642683U, /* GTGTGTGTGTGTGTGT */
   3204181950U, /* GTTGTTGTTGTTGTTG */
   3272356035U, /* TAATAATAATAATAAT */
   3340530119U, /* TACTACTACTACTACT */
   3408704203U, /* TAGTAGTAGTAGTAGT */
   3435973836U, /* TATATATATATATATA */
   3476878287U, /* TATTATTATTATTATT */
   3545052371U, /* TCATCATCATCATCAT */
   3613226455U, /* TCCTCCTCCTCCTCCT */
   3681400539U, /* TCGTCGTCGTCGTCGT */
   3722304989U, /* TCTCTCTCTCTCTCTC */
   3749574623U, /* TCTTCTTCTTCTTCTT */
   3817748707U, /* TGATGATGATGATGAT */
   3885922791U, /* TGCTGCTGCTGCTGCT */
   3954096875U, /* TGGTGGTGGTGGTGGT */
   4008636142U, /* TGTGTGTGTGTGTGTG */
   4022270959U, /* TGTTGTTGTTGTTGTT */
   4090445043U, /* TTATTATTATTATTAT */
   4158619127U, /* TTCTTCTTCTTCTTCT */
   4226793211U, /* TTGTTGTTGTTGTTGT */
   4294967295U, /* TTTTTTTTTTTTTTTT */
  };


EF64_T
Repetitive_setup (int index1part) {
  Oligospace_T *repetitive_oligos;

  if (index1part == 6) {
    repetitive_oligos = repetitive_oligos_6;
  } else if (index1part == 7) {
    repetitive_oligos = repetitive_oligos_7;
  } else if (index1part == 8) {
    repetitive_oligos = repetitive_oligos_8;
  } else if (index1part == 9) {
    repetitive_oligos = repetitive_oligos_9;
  } else if (index1part == 10) {
    repetitive_oligos = repetitive_oligos_10;
  } else if (index1part == 11) {
    repetitive_oligos = repetitive_oligos_11;
  } else if (index1part == 12) {
    repetitive_oligos = repetitive_oligos_12;
  } else if (index1part == 13) {
    repetitive_oligos = repetitive_oligos_13;
  } else if (index1part == 14) {
    repetitive_oligos = repetitive_oligos_14;
  } else if (index1part == 15) {
    repetitive_oligos = repetitive_oligos_15;
  } else if (index1part == 16) {
    repetitive_oligos = repetitive_oligos_16;
  } else {
    fprintf(stderr,"Repetitive_setup can handle only values of 6 through 16\n");
    abort();
  }

  return EF64_new_from_oligos(repetitive_oligos,NREPETITIVE,/*oligosize*/index1part);
}

