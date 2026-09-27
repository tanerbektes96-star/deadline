# DEADLINE — Karakter Üretim Hattı (Tripo → Blender → Unreal)

**Kapsam:** GDD 1 ("8 benzersiz + 10 varyant"), GDD 3.1 (birinci şahıs), GDD 7.9 (devlet NPC'leri), GDD 10.4 (personel), Yol Haritası Ay 5 (Manny/Quinn placeholder), Ay 6 (ambient yaya), Ay 9 (gerçek modeller).

> **Takvim notu:** Yol haritasına göre gerçek karakter modelleri **9. ayın** işi; 5. ayda
> NPC'ler Manny/Quinn ile kurulur. Bu yüzden şimdilik önerim, **tek bir karakterle
> (V01 Depo Görevlisi) hattı baştan sona denemek**, geri kalanını 9. aya bırakmak.
> Aşağıdaki yöntem bunu mümkün kılıyor: bütün karakterler Manny kemik hiyerarşisini
> kullandığı için 5. ayda Manny ile yazılan AnimBP ve AI kodu, 9. ayda mesh değişince
> aynen çalışmaya devam eder.

> **Durum (2026-09-27): V01 ile hat baştan sona çalışıyor.** V01 riglendi, Unreal'e
> aktarıldı, ortak animasyon seti retarget edildi ve `LVL_Greybox`'ta NavMesh üzerinde
> raf → tedarikçi → alıcı arasında devriye geziyor. Bölüm 5 ve 6'daki **"Gerçekleşen"**
> kutuları, planın uygulamada nasıl kurulduğunu ve yolda çıkan tuzakları anlatır;
> V02 ve sonrası bunlara göre yapılır.

---

## 1. Seçilen Yöntem ve Gerekçesi

### 1.1 Tek iskelet: `SK_Deadline_Human` (Manny hiyerarşisi, T-pose)

Bütün karakterler, **UE5 Manny'nin kemik isimleri ve hiyerarşisiyle** (`root → pelvis → spine_01…05 → neck_01 → head`, `clavicle_l → upperarm_l → lowerarm_l → hand_l`, parmaklar, `thigh_l → calf_l → foot_l → ball_l` + sağ taraf) Blender'da riglenir. Bütün karakterler T-pose olduğu için Unreal'de **tek bir ortak Skeleton asset'i** paylaşırlar.

| Getirisi | Neden önemli |
|---|---|
| Tek AnimBP bütün NPC'lere yeter | 18 karakter için 1 animasyon mantığı |
| Retarget **bir kez** yapılır (Manny → SK_Deadline_Human) | Her yeni karakter için tekrar iş yok |
| Blender'da yaptığım özel animasyonlar herkeste çalışır | Denetçinin sayım animasyonu, istenirse depo görevlisinde de oynar |
| Leader Pose Component, Animation Budget Allocator uyumlu | GDD 19'daki NPC performans planıyla aynı |
| 5. aydaki Manny placeholder'ları sorunsuz değişir | Kod tarafında sıfır değişiklik |

İskelet Manny'nin **88 kemiğinin tamamını** taşır; `ik_foot_*`, `ik_hand_*`, `interaction`, `center_of_mass` dahil. Bunlar deform etmez ama atılmaz: `ABP_NPC_Base`'in ayak IK'sı (`CR_Mannequin_FootIK`) `ik_foot_*` kemiklerini hedef olarak kullanır.

**Neden Tripo'nun kendi auto-rig'i değil?** Tripo'nun rig'i her karakter için ayrı, standart dışı bir iskelet üretir; her biri için ayrı retarget ve ayrı animasyon kopyası gerekir. Tripo rig'i sadece **yedek plan** olarak kalır.

### 1.2 Animasyonların kaynağı — dürüst dağılım

Script ile elle anahtar kare (keyframe) atılmış bir yürüyüş döngüsü, motion capture kalitesine yetişmez. En iyi sonuç için iş şöyle bölünür:

| Animasyon türü | Kaynak | Kim yapar |
|---|---|---|
| Idle, yürüme, koşma, dönme, başla/dur | **Unreal Third Person paketi** (Manny `MM_*` animasyonları) + istenirse Epic'in ücretsiz **Game Animation Sample (GASP)** paketi — mocap kalitesinde | Unreal'de IK Retargeter ile tek seferde |
| Yaygın jestler (telefon, kutu taşıma, not alma, bekleme, oturma) | **Mixamo** (ücretsiz, Adobe hesabı gerekir, indirmeyi sen yaparsın) | Unreal'de IK Retargeter |
| Role özel kısa döngüler ve boşlukları doldurma | Blender | **Ben, MCP üzerinden** |
| Döngü düzeltme, kök hareketi (root motion) temizliği, Mixamo'da olmayan varyantlar | Blender | **Ben** |

Kısacası: hareket temeli mocap'ten, karakteri karakter yapan jestler benden.

### 1.3 Oyuncu karakteri

GDD 3.1: oyun tamamen birinci şahıs, **sadece el ve kol modeli** gerekiyor. İki seçenek:

- **Önerilen:** Oyuncu için ayrı bir Tripo modeli üretme. Kol modelini `SK_Deadline_Human`'dan (P01 gövdesinden) kesip birinci şahıs kolu yaparız; kutu taşıma pozlarını Blender'da ben yaparım.
- P01 prompt'u yine de aşağıda var — gölge, fragman veya ileride "true first person" (gövdesini aşağı bakınca görme) istersen kullanılır.

---

## 2. Tripo'ya Girmeden Önce: Görsel Kuralları

Tripo'da **metinden 3B** yerine **görselden 3B** kullan. Önce aşağıdaki prompt'larla bir görsel üretici (Tripo'nun kendi text-to-image'ı, GPT-image, Midjourney vb.) ile görsel al, sonra o görseli Tripo'ya ver. Görsel üzerinde kontrolün çok daha yüksek olur.

### 2.1 Ortak stil bloğu (her prompt'un başına ekle)

Bütün karakterlerin aynı oyundan çıkmış gibi görünmesi için **hiç değiştirmeden** kullan:

```
Full-body 3D game character concept, front view, strict T-pose: arms straight out
horizontally at shoulder height, palms facing down, fingers together and straight,
legs straight and slightly apart, feet flat on the ground pointing forward.
Semi-realistic stylized style: realistic adult proportions (7.5 heads tall), slightly
simplified shapes, clean readable silhouette, clear material separation between
skin, fabric, leather and rubber. Contemporary fictional port city, grounded
working-class realism, muted desaturated palette. Neutral relaxed face, mouth closed,
eyes open looking straight ahead. Even flat studio lighting, no cast shadows,
plain light grey seamless background. Entire body visible from top of head to
shoes, centered, orthographic camera at chest height, no perspective distortion.
Hands empty. Clothing fitted or medium fit, hair close to the head.
```

### 2.2 Ortak negatif prompt

```
props in hands, bags, backpack, bag straps, weapons, long coat below the knee, skirt,
dress, cape, scarf, loose hanging fabric, flowing long hair, A-pose, arms down,
crossed arms, dynamic pose, perspective distortion, fisheye, cropped feet, cropped
head, multiple characters, text, logo, real flags, real police or government
insignia, watermark, dramatic lighting, strong shadows, rim light, background
scenery, motion blur, cartoon, anime, chibi
```

**Neden bu kurallar:**

| Kural | Sebep |
|---|---|
| Elde eşya yok | Pano, tablet, kamera, poşet **ayrı static mesh** olur ve eldeki socket'e takılır. Tripo eşyayı ele kaynak yapar, sökmek iş çıkarır |
| Etek, uzun palto, pelerin, atkı yok | Bacak arası birleşik kumaş rig'de yırtılır/çekilir. Kumaş simülasyonu kapsam dışı |
| Uzun dökümlü saç yok | Saç fiziği yok; kafaya yakın saç kafa kemiğine bağlanır, sorunsuz |
| Düz ışık, gölgesiz | Tripo gölgeyi dokuya "pişirir"; oyunda çift gölge olur |
| Gerçek amblem, bayrak, polis yazısı yok | Kurgusal şehir; GDD sonundaki telif notu. Kurumlar renk ve şeritle ayrılır |
| Açık gri zemin | Tripo'nun arka plan ayırması en temiz bu zeminde |

### 2.3 Renk dili (karakterler arası okunabilirlik)

Oyuncu depoda gezerken kimin kim olduğunu **renkten** anlamalı:

| Grup | Renk kodu |
|---|---|
| **Şirket personeli** (bizim çalışanlar) | Kömür gri iş ceketi/yelek + **mavi-yeşil (teal) şerit** — UI'daki başarı rengiyle aynı |
| **Gümrük** | Lacivert gömlek, apolet, sade şapka |
| **Yol kontrol** | Koyu gri-mavi üniforma + floresan sarı reflektörlü yelek |
| **Şehir denetçisi / müfettiş** | Sivil: gri-bej takım, boyunda kimlik kartı askısı |
| **Gri dünya** | Markasız, soluk, yıpranmış iş kıyafeti (GDD: "gri malın markası yoktur" kuralının karakterdeki karşılığı) |
| **Ambient yaya** | Sivil, doygunluğu düşük gündelik renkler |

---

## 3. Karakter Listesi ve Prompt'lar

Toplam **8 benzersiz + 10 varyant** (GDD 1). Boy değerleri Blender'da ölçeklemede kullanılır.

> Kullanım: **[Ortak stil bloğu] + [karakter prompt'u]**, negatif alana **[ortak negatif]**.

### 3.1 Benzersiz karakterler (8)

#### P01 — Oyuncu (opsiyonel, bkz. 1.3)
Erkek, 30'lar, Türk/Akdenizli · 180 cm
```
A 34-year-old Turkish man, owner of a small logistics company, average athletic build,
short dark hair, short trimmed stubble. Charcoal grey work jacket with a thin teal
stripe on the shoulders, dark grey henley shirt underneath, dark navy work trousers
with knee panels, brown leather work boots.
Practical, tired but determined look.
```

#### G01 — Şehir Denetçisi (N17)
Kadın, 50'ler, Anadolu/Türk · 165 cm · *Bürokratik, öngörülebilir, kurallara harfiyen uyar*
```
A 54-year-old Turkish woman, city inspector, medium build, short neat grey-streaked
dark hair in a low bun, thin rectangular reading glasses. Beige-grey tailored blazer
over a white collared blouse, straight grey trousers, flat black leather shoes. An ID
card on a plain blue lanyard around her neck, card blank without text. Serious,
orderly, meticulous expression.
```
Ayrı prop: klip panolu evrak (`SM_Clipboard`), tükenmez kalem.

#### G02 — Gümrük Memuru
Erkek, 40'lar, Kuzey Afrikalı (Mağrip) · 178 cm · *Limanda, konteyner açar, "öncelik ücreti" alabilir*
```
A 45-year-old North African man, port customs officer, stocky build, short black hair
greying at the temples, neat full moustache. Navy blue short-sleeve uniform shirt with
plain epaulettes and no insignia, navy trousers, black belt, black boots, a plain navy
peaked cap sitting straight on the head. Calm, knowing, slightly transactional expression.
```
Ayrı prop: el feneri, mühür, tablet.

#### G03 — Vergi Müfettişi
GDD 7.9: *"Depoya hiç gelmez. Rakamlara bakar."* → **3B model üretme.** UI'da sadece portre. Portre için aynı stil bloğunu "front view, T-pose" yerine "head and shoulders portrait" ile kullan:
```
Head and shoulders portrait, a 60-year-old East Asian man, tax auditor, thin face,
neatly combed grey hair, wire-rimmed glasses, dark charcoal suit, white shirt,
dark tie. Cold, patient, analytical expression. Plain dark background.
```

#### G04 — Yol Kontrol Ekibi
Erkek, 30'lar, Doğu Avrupalı/Slav · 185 cm · *Seyahat sırasında rastgele çıkar*
```
A 36-year-old Eastern European man, road checkpoint officer, tall and broad-shouldered,
very short light brown hair, clean shaven. Dark blue-grey uniform jacket with plain
shoulder patches without text, fluorescent yellow reflective safety vest over the
jacket, dark blue-grey cargo trousers, black tactical boots, plain dark cap.
Impassive, bored authority expression.
```
Ayrı prop: dur işareti lolipop, el feneri, irsaliye.

#### N16 — Muhabir
Kadın, 20'ler sonu, Doğu Asyalı (Koreli) · 162 cm · *Fiyat fahişliğini haber yapar*
```
A 28-year-old Korean woman, investigative city journalist, slim build, shoulder-length
straight black hair tucked behind the ears, no fringe covering the eyes. Olive green
waist-length rain jacket, grey hooded sweatshirt underneath, dark slim jeans, white
sneakers. A blank press badge clipped to the jacket. Sharp, curious, alert expression.
```
Ayrı prop: fotoğraf makinesi, telefon, çapraz çanta.

#### T01 — Tezgâh Tedarikçisi
Erkek, 60'lar, Levanten/Orta Doğulu · 172 cm · *İlk kanal; oyuncunun ilk yüz yüze ticareti*
```
A 63-year-old Levantine man, wholesale market stall supplier, heavy set, bald on top
with short grey hair on the sides, thick grey moustache. Brown knitted cardigan over a
checkered button shirt with rolled sleeves, a dark canvas apron tied at the waist
ending above the knees, brown trousers, worn leather shoes. Warm, shrewd merchant smile
with mouth closed.
```
Ayrı prop: hesap makinesi, para destesi.

#### T02 — Liman Aracısı (ilk gri teklif, GDD 7.10 Hedef 2)
Kadın, 40'lar, Batı Avrupalı · 170 cm
```
A 44-year-old Western European woman, independent dockside middleman, lean wiry build,
weathered skin, ash blonde hair tied back in a tight short ponytail. Faded dark grey
waxed work jacket without any brand, worn black hoodie underneath, stained grey cargo
trousers, scuffed steel-toe boots. Unbranded, used, everything slightly faded.
Guarded, calculating, half-friendly expression.
```

#### T03 — Piyasa Brokeri (rapor satan)
Erkek, 30'lar, Güney Asyalı (Hint) · 175 cm · *GDD 4: "brokerdan rapor satın alma"*
```
A 35-year-old South Asian man, freelance market information broker, slim build, neat
short black hair with a side part, short well-groomed beard. Light blue button-down
shirt with sleeves rolled to the forearm, navy knit vest, grey chinos, brown leather
loafers, a thin smartwatch. Fast-talking, confident, slightly smug expression.
```
Ayrı prop: tablet, kahve bardağı.

### 3.2 Varyantlar (10)

Personelin hepsi aynı şirket kıyafetini giyer (kömür gri + teal şerit) — oyuncu kendi çalışanını tek bakışta tanır.

#### V01 — Depo Görevlisi A ⭐ *hattın ilk test karakteri*
Erkek, 20'ler, Batı Afrikalı (Nijeryalı) · 183 cm
```
A 26-year-old Nigerian man, warehouse worker, athletic build, short black hair with a
low fade, clean shaven. Charcoal grey work jacket with teal reflective stripes on the
chest and arms, grey t-shirt underneath, dark grey cargo work trousers with knee pads,
black safety boots with rubber soles. Friendly, focused expression.
```
Neden ilk bu: sade silüet, eşyasız, açık kollar — rig testi için ideal.

#### V02 — Depo Görevlisi B
Kadın, 30'lar, Latin Amerikalı · 163 cm
```
A 32-year-old Latin American woman, warehouse worker, sturdy build, dark brown hair
tied in a tight low bun. Charcoal grey work vest with teal reflective stripes over a
long-sleeve dark grey work shirt, dark grey cargo trousers, black safety boots.
Energetic, capable expression.
```

#### V03 — Transpalet / Forklift Operatörü
Erkek, 50'ler, Türk · 174 cm
```
A 55-year-old Turkish man, pallet jack and forklift operator, heavy stocky build, short
greying hair, thick dark moustache. Charcoal grey work overalls with teal stripes on
the legs and chest, sleeves rolled up, black safety boots. Calm, experienced, unhurried
expression.
```

#### V04 — Şoför (Kıdemli Şoför = aynı mesh, farklı doku)
Erkek, 40'lar, Orta Asyalı (Kazak/Özbek) · 176 cm
```
A 42-year-old Central Asian man, delivery truck driver, medium build, short black hair,
light stubble. Charcoal grey zip-up work fleece with a teal stripe across the chest,
dark jeans, brown work boots. Patient, road-worn expression.
```
Kıdemli şoför: Unreal'de Material Instance ile ceket rengi koyu, saç griye (tint).

#### V05 — Tamirci
Kadın, 30'lar, Doğu Avrupalı · 168 cm
```
A 38-year-old Eastern European woman, vehicle mechanic, strong build, short dark auburn
hair in a pixie cut. Charcoal grey mechanic coveralls with teal stripes, the top half
unzipped and sleeves tied around the waist tightly, a dark grey tank top, black boots,
faint grease marks on the forearms. No-nonsense, practical expression.
```

#### V06 — Ofis Personeli (Satın Alma / Analist / Uyum Sorumlusu, dokuyla ayrışır)
Erkek, 30'lar, Çinli · 172 cm
```
A 33-year-old Chinese man, logistics office employee, slim build, short neat black hair,
thin black glasses. White button shirt with a charcoal grey sweater vest showing a small
teal stripe on the collar, dark grey trousers, black leather shoes. Blank ID card on a
teal lanyard. Attentive, precise expression.
```

#### V07 — Yaya: Yaşlı Kadın
Kadın, 60'lar, Akdenizli · 158 cm
```
A 66-year-old Mediterranean woman, local resident, short and plump, short curly grey
hair. Long-sleeve maroon knitted cardigan reaching the hips, patterned blouse, dark
brown straight trousers, comfortable black walking shoes.
```
Ayrı prop: file çanta.

#### V08 — Yaya: Genç Erkek
Erkek, 20'ler, Afro-Avrupalı · 180 cm
```
A 22-year-old Black European man, city resident, slim build, short twisted hair.
Muted mustard hoodie, black track pants, white-grey sneakers, small earbuds.
```
Ayrı prop: sırt çantası (sonradan socket ile).

#### V09 — Yaya: Ofis Çalışanı Kadın
Kadın, 40'lar, Güneydoğu Asyalı (Filipinli) · 160 cm
```
A 41-year-old Filipino woman, office worker commuting, medium build, chin-length black
bob haircut. Dark navy blazer, cream blouse, grey tapered trousers, black low-heel shoes.
```

#### V10 — Yaya: Emekli Erkek
Erkek, 70'ler, Kürt/Orta Doğulu · 170 cm
```
A 72-year-old Middle Eastern man, retired local, thin slightly stooped build, white short
hair, white moustache, flat tweed cap. Grey wool vest over a light blue shirt, brown
trousers with a belt, old brown leather shoes.
```
Ayrı prop: baston.

> **Çeşitlilik çarpanı:** Her varyantın Unreal'de 2–3 Material Instance'ı olur (kıyafet
> tonu, saç rengi). 10 mesh × 3 = sokakta 30 farklı görünüm, ek model üretmeden.

---

## 4. Tripo Ayarları

| Ayar | Değer |
|---|---|
| Mod | **Image to 3D** (varsa multiview: aynı prompt'la ön + arka görsel üret) |
| Model sürümü | En güncel |
| Doku | HD / 2K, **PBR açık** |
| Topoloji | **Quad** (rig ve deformasyon için daha iyi) |
| Poligon hedefi | NPC: 15–25 bin üçgen · P01: 30 bin |
| Poz | T-pose (görseldeki gibi) |
| Auto-rig | **Kapalı** — rig'i Blender'da Manny iskeletine yapacağım |
| Dışa aktarım | **GLB** (dokular gömülü) |

Çıktı kontrol listesi — Tripo'dan indirmeden önce:
- [ ] Parmaklar birbirine kaynamamış (kaynamışsa yeniden üret; parmak rigi zorlaşır)
- [ ] Kol altı ve bacak arası açık, gövdeye yapışık değil
- [ ] Yüz simetrik, gözler düzgün
- [ ] Dokuda gölge "pişmemiş"

**Dosya yeri:** Ham dosyaları `Content/` dışına koy, Unreal her dosyayı içeri almaya çalışmasın:

```
DEADLINE_/ArtSource/Characters/V01_DepoGorevlisiA/V01_tripo.glb
DEADLINE_/ArtSource/Characters/V01_DepoGorevlisiA/V01_ref.png   (Tripo'ya verdiğin görsel)
```

---

## 5. Blender Aşaması (MCP ile benim yapacaklarım)

1. GLB içe aktarma, gerçek boya ölçekleme (bölüm 3'teki cm değerleri), dönüşümleri uygulama, normalleri düzeltme, gerekirse poligon azaltma.
2. Manny iskeletini (senin Unreal'den dışa aktaracağın `SK_Mannequin.fbx`) içe aktarma, T-pose'a çevirip rest pose olarak uygulama.
3. Eklemleri mesh'e oturtma (omuz, dirsek, bilek, kalça, diz, ayak bileği, parmaklar) — mesh sınırlarından otomatik ölçüm + gözle düzeltme.
4. Otomatik ağırlık (weight) + sorunlu bölgeleri elle düzeltme: omuz, kalça, dirsek arkası, parmaklar.
5. Deformasyon testi: kolları indir, çömeltme, yürüme pozu — render alıp sana gösteririm.
6. Özel animasyonlar (bölüm 7).
7. FBX dışa aktarım: Unreal ölçeğine uygun, `Add Leaf Bones` kapalı, sadece deform kemikleri.

> **Gerçekleşen (V01):** Blender MCP oturum başında bağlanamadı (timeout); iş, Blender 5.2'yi
> arka planda çalıştıran tek bir script'le yapıldı — tekrar çalıştırılabilir, elle adım yok:
>
> ```
> blender -b --factory-startup V01_source.blend --python ArtSource/Tools/rig_v01.py
> ```
>
> Çıktılar `ArtSource/Characters/V01_DepoGorevlisiA/` altında: `V01_rig.blend`,
> `SKM_V01_DepoGorevlisiA.fbx`. Plandan farklar ve tuzaklar:
>
> | Konu | Ne yapıldı / neden |
> |---|---|
> | Ölçek | Tripo mesh'i ~1 m'ye normalize veriyor; bölüm 3'teki boya (V01: 183 cm) ölçeklenir |
> | Poz | Tripo T-pose'u korunur. Manny iskeleti **T-pose'a çevrilip** mesh'e oturtulur, rest pose olarak kalır. Kemik eksenleri Manny'ninkiyle aynı tutulur |
> | Eklemler | Mesh kesitlerinden ölçülüp script'e **elle** yazılır (omuz, dirsek, bilek, kalça, diz, ayak, parmaklar). Her karakter için yeniden ölçülür |
> | Ağırlık | Otomatik ağırlık **yerine**: Manny mesh'i yeni rest pozuna bükülür, UE kalitesindeki ağırlıkları V01'e aktarılır; sonra yumuşatma, kemik başına en fazla 4 etki, normalize |
> | Kemikler | 88 kemiğin tamamı dışa aktarılır (bkz. 1.1); "sadece deform" uygulanmadı |
> | **Birim — kritik** | FBX **santimetre** ile yazılır (sahne birim ölçeği 0.01, rig ve mesh ×100 uygulanmış). Metreyle yazılınca dönüşüm `root` kemiğine ×100 ölçek olarak gömülüyor; bind pozda her şey doğru görünürken retarget edilen animasyonlarda iskelet pelvisin içine çöküyor |
> | Test | Deformasyon testi render'la yapılır (kolları indir, dirsek, diz, gövde bükme) |
>
> Bilinen eksik: eldivenli, birleşik parmaklar low-poly; yakın el pozlarında kaba görünebilir.

---

## 6. Unreal Aşaması

1. **Third Person içeriğini ekle:** Content Drawer → Add → *Add Feature or Content Pack* → **Third Person**. Manny/Quinn ve `MM_Idle`, `MM_Walk_Fwd`, `MM_Run_Fwd` gibi animasyonlar gelir.
2. (Opsiyonel, önerilir) Fab'dan **Game Animation Sample** — yüzlerce mocap locomotion animasyonu. 5.8 desteğini Fab sayfasından kontrol et.
3. İlk karakteri içe aktar → Skeleton olarak yeni `SK_Deadline_Human` oluşur. **Sonraki bütün karakterlerde bu Skeleton seçilir.**
4. IK Rig + IK Retargeter: Manny → SK_Deadline_Human (hiyerarşi aynı olduğu için neredeyse otomatik). Animasyonları toplu retarget et.
5. `ABP_NPC_Base`: Idle / Walk / Run blend space + role özel slot (montage) katmanı.
6. Klasörler (Yol Haritası 2.3 yapısına uygun):

```
Content/Deadline/Characters/
  Shared/        SK_Deadline_Human, ABP_NPC_Base, BS_Locomotion, IK_*, RTG_*
  Shared/Anims/  Retarget edilmiş ortak animasyonlar
  NPC/<ID>/      Mesh, materyaller, role özel animasyonlar
  Player/        FP kollar
  Props/         Pano, tablet, kamera, fener, baston…
```

> **Gerçekleşen (V01):** Hepsi script'le kurulur. Editör kapalıyken:
>
> ```
> ArtSource/Tools/run_ue_setup_v01.ps1
> ```
>
> Önce `Shared/`, `NPC/V01/` ve `Maps/Test/` klasörlerini Geri Dönüşüm Kutusu'na taşır,
> sonra `ue_setup_v01.py`'yi çalıştırır. Oluşanlar:
>
> | Yer | İçerik |
> |---|---|
> | `Shared/` | `SK_Deadline_Human`, `IK_Manny`, `IK_Deadline_Human`, `RTG_Manny_to_Deadline_Human`, `BS_Locomotion`, `ABP_NPC_Base` |
> | `Shared/Anims/` | Third Person **Unarmed** setinin 26 animasyonu: Idle, 16 yönlü Walk/Jog, Jump/Fall/Land, Dash, WallJump, Attack'lar |
> | `NPC/V01/` | `SKM_V01_DepoGorevlisiA`, Physics Asset, materyal + dokular, `BP_NPC_V01` |
>
> `ABP_NPC_Base` ve `BS_Locomotion`, Third Person şablonundaki `ABP_Unarmed` ve `BS_Idle_Walk_Run`'ın
> retarget edilmiş kopyalarıdır. Role özel slot (montage) katmanı henüz eklenmedi. Game Animation
> Sample (madde 2) eklenmedi.
>
> **Yolda çıkan tuzaklar (UE 5.8):**
>
> | Belirti | Sebep ve çözüm |
> |---|---|
> | Oyunda bacaklar adım atmıyor, dizler çömelir gibi bükülüyor; üst gövde doğru yürüyor | Retargeter `ik_foot_*` kemiklerini taşımıyor, T-pozda kalıyorlar; ayak IK'sı ayakları oraya çekiyor. Retargeter'a **Pin Bones** adımı eklendi: `ik_foot_l/r ← foot_l/r`, `ik_hand_* ← hand_*`. Blender'daki kontrollerde görünmez, sadece oyun içinde çıkar |
> | Oyunda anim instance hiç oluşmuyor | Skeleton `Shared/`'a taşınınca mesh'teki referans sadece bellekte güncellendi; mesh yeniden kaydedilmeyince diskte ölü yolu gösteriyordu. Script taşımadan sonra mesh'i zorla kaydediyor ve kontrol ediyor |
> | Physics Asset oluşmuyor | Yeni FBX importer'ı (Interchange) atlıyor. Import eski FBX yolundan yapılıyor (`Interchange.FeatureFlags.Import.FBX 0`) |
> | Metallic dokusu gelmiyor | Eski FBX importer'ı almıyor. V01'de değeri neredeyse sıfır (kumaş), etkisi yok. Metal ağırlıklı bir karakterde elle bağlanmalı |
> | Aynı oturumda silip yeniden kurunca eksik/eski asset kalıyor | Silme işi editör açılmadan diskte yapılıyor (`run_ue_setup_v01.ps1`) |
>
> **NPC hareketi (C++, `Source/DEADLINE_/NPC/`):**
>
> - `ADeadlineNPCCharacter` — bütün yürüyen NPC'lerin temeli. Level'daki her örneğe
>   `PatrolPoints`, `PauseAtPoint`, `WanderRadius` verilir. Hız 300 cm/s: blendspace'teki yürüme
>   örneği bu hızda, farklı hızda ayaklar kayar. Yol takibi ivmeyle yürür; yoksa ABP karakteri
>   duruyor sayar ve NPC Idle pozunda kayar.
> - `ADeadlineNPCController` — Tick'siz. Noktaları sırayla gezer, nokta yoksa NavMesh üzerinde
>   rastgele dolaşır, ulaşamadığı noktayı log'a yazıp sonrakine geçer.
> - Yeni karakter = `DeadlineNPCCharacter`'dan türeyen yeni bir Blueprint + farklı mesh.
>
> **Level'a yerleştirme:** `Content/Deadline/Core/setup_npc_v01.py`, `LVL_Greybox`'a depo
> NavMesh'ini, üç devriye noktasını ve V01'i ekler. Tekrar çalıştırılabilir, elle kaydırılmış
> actor'lara dokunmaz. Editör içinden *Tools → Execute Python Script* ile çalışır.
>
> **Testler:** `ArtSource/Tools/ue_test_npc_v01.py` ayrı bir test level'ında
> (`Maps/Test/LVL_Test_NPC_V01`) iki nokta arasına duvar koyup NPC'nin etrafından dolaştığını ve
> adım attığını ölçer. `-ExecCmds="py <dosya>"` ile çalıştırılır; `-ExecutePythonScript` testi
> bitmeden editörü kapatır.

---

## 7. Animasyon Listesi

**Ortak (retarget, herkes):** Idle ×2 varyant, Walk, Jog/Run, sağ/sol dönme, başla/dur.

| Karakter | Özel animasyon | Kaynak önerisi | Oyundaki anı |
|---|---|---|---|
| G01 Denetçi | Panoya not alma (döngü), rafa işaret edip sayma, kapıyı çalma | Mixamo + ben | Rutin denetim, el koyma sekansı |
| G02 Gümrük | Fenerle konteyner içine bakma, belgeye mühür basma | Ben | Liman kontrolü |
| G04 Yol kontrol | "Dur" el işareti, aracın etrafında yürüyüp irsaliye okuma | Mixamo + ben | Seyahat olayı |
| N16 Muhabir | Fotoğraf çekme, telefonla kayıt | Mixamo + ben | Fiyat fahişliği haberi |
| T01 Tedarikçi | Tezgâh arkası idle, tezgâha yaslanma, para sayma | Ben | Tezgâh kanalı |
| T02 Aracı | Duvara yaslanma, etrafı kolaçan etme, "gel" işareti | Mixamo + ben | İlk gri teklif |
| T03 Broker | Tabletle konuşma, anlatırken el jestleri | Mixamo | Bilgi ürünü satışı |
| Personel (V01–V05) | **Kutu kaldırma / taşıma / bırakma**, transpalet itme, rafa koyma | Mixamo + ben (taşıma üst gövde katmanı) | Otomasyon merdiveni 1–5 |
| V06 Ofis | Masada klavye, oturma idle | Mixamo | Ofis |
| Yayalar (V07–V10) | Telefona bakma, etrafa bakınma, bekleme | Mixamo | Ay 6, 3 durumlu yaya |
| Denetim ekibi | Kutu taşıyarak depodan çıkma (personel taşımasının tekrarı) | Paylaşımlı | **El koyma sekansı** (GDD 7.6) |

---

## 8. Sıradaki Adımlar

| # | Kim | İş | Durum |
|---|---|---|---|
| 1 | Sen | Blender kur (4.2+ LTS), Blender MCP eklentisini kur ve bağla (bölüm 9) | ✅ Blender 5.2 |
| 2 | Sen | Unreal'e Third Person paketini ekle; `SK_Mannequin`'i FBX olarak `ArtSource/Mannequin/` altına dışa aktar | ✅ |
| 3 | Sen | V01 görselini üret → Tripo → GLB'yi `ArtSource/Characters/V01_DepoGorevlisiA/` altına koy | ✅ |
| 4 | Ben | Blender'da V01'i temizle, Manny iskeletine rigle, deformasyon testini göster | ✅ 2026-09-26 |
| 5 | Sen + ben | Unreal'e aktar, retarget, V01 depoda yürüsün | ✅ 2026-09-27 |
| 6 | — | Hat çalışıyorsa kalan 17 karakter aynı adımlarla (9. ayda) | Bekliyor |

**V02 ve sonrası için farklar.** Bölüm 6'daki kurulum script'i V01'e özeldir: `Shared/`
klasörünü her çalıştırmada silip baştan kurar. Sonraki karakterler **mevcut
`SK_Deadline_Human`'ın üstüne** import edilecek; bunun için ayrı, `Shared/`'a dokunmayan bir
import script'i yazılacak. Blender tarafında `rig_v01.py` kopyalanır; boy ve eklem ölçüleri
karaktere göre yeniden girilir.

## 9. Blender MCP Kurulumu

1. Blender'ı kur.
2. `blender-mcp` eklentisinin `addon.py` dosyasını indir (GitHub: `ahujasid/blender-mcp`). Blender → Edit → Preferences → Add-ons → *Install from Disk* → `addon.py` → etkinleştir.
3. Blender'da 3B görünümde **N** → *BlenderMCP* sekmesi → **Connect to Claude** (sunucu başlar).
4. Claude Code'a MCP sunucusunu ekle (`uv` makinende zaten kurulu):

   ```
   claude mcp add blender -- uvx blender-mcp
   ```

5. Oturumu yeniden başlat; bana "Blender bağlı mı?" diye sor, sahneyi okuyarak kontrol ederim.

> **Not:** MCP bağlantısı, açık Blender'daki sahneyi canlı görmek ve düzenlemek için gerekir.
> Rig ve export gibi tekrarlanabilir işler, MCP bağlı olmasa da arka planda çalışan Blender
> script'leriyle yapılabilir (bölüm 5). Proje kökündeki `.mcp.json` sunucuyu tanımlar.
