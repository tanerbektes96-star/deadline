# DEADLINE — Oyun Tasarım Belgesi v2.0

**Alt başlık:** Grey Market Logistics Simulator
**Motor:** Unreal Engine 5
**Bakış açısı:** Birinci şahıs
**Ekip:** 1 geliştirici
**Çıkış modeli:** Early Access → içerik güncellemeleri → 1.0
**Bu sürümde değişen:** v1.0'daki kapsam sorunları düzeltildi, gri pazar sistemi eklendi, ürün kataloğu yeniden yapılandırıldı, manuel araç kullanımı kaldırıldı.

> **Not:** Tanımadığınız bir terim görürseniz `DEADLINE_Sozluk.md` dosyasına bakın. Bu belgede terimler ilk geçtiklerinde parantez içinde kısaca açıklanır.

---

## 0. En Kısa Özet

Küçük bir tedarik şirketi işletiyorsunuz. Ucuza mal alıp depolayıp pahalıya satıyorsunuz. Kriz geldiğinde elinizde doğru mal varsa zengin oluyorsunuz.

Oyunun ayırt edici tarafı iki şey:

1. **Öngörü.** Piyasada ne olacağını herkesten önce tahmin edip paranızı ona bağlıyorsunuz.
2. **Gri pazar.** Daha çok kazanmak için kayıt dışına çıkabilirsiniz — ama devlet denetliyor.

Her şey fiziksel: kutuları kendiniz taşıyor, rafa kendiniz diziyor, kamyona kendiniz yüklüyorsunuz. Şirket büyüdükçe bu işleri çalışanlara devrediyorsunuz.

---

## 1. Değişiklik Özeti (v1.0 → v2.0)

| Konu | v1.0 | v2.0 | Sebep |
|---|---|---|---|
| Yapı | 8 bölümlük kapalı kampanya | Sandbox çekirdek + üstüne binen hedef zinciri | Kampanya üretimi tek kişi için 3 kat fazla iş |
| Çıkış | 12. ayda tam sürüm | 10. ayda doyurucu Early Access | Tür standardı; wishlist ve geri bildirim |
| Araç kullanımı | Şehirde manuel sürüş | **Kaldırıldı.** Araca bin → "Seyahat Et" → haritadan seç | En büyük gizli maliyet kalemi |
| Yol ağı | Sürülebilir yol grafiği | Sadece yürünebilir şehir | Trafik AI, çarpışma, hasar sistemleri iptal |
| Ürün sayısı | 50 | 63 tasarlandı, 34'ü çıkışta | Elektronik bileşen ve lisanslı mal katmanları eklendi |
| Ürün kimliği | Jenerik isimler | 24 kurgusal marka + 3 kalite kademesi | Kimlik, hafızada kalma, ücretsiz içerik çeşitliliği |
| Depolama ölçüsü | Karışık (kg/L/adet/palet) | **BU (Box Unit)** tek birim | Kapasite matematiği kurulabilir hâle geldi |
| Kap tipi | Tanımsız | 7 kap tipi, tamamı BU'ya çevrilir | 63 ürün için 7 model yeterli |
| Kara borsa | Sadece alt başlıkta | **Tam sistem:** iki defter, lisans, denetim, şüphe | Farklılaştırıcı ve klip üreticisi |
| Devlet | 1 pasif NPC | 4 aktif denetim rolü + 4 denetim türü | Riskin gerçek olması için |
| Fiyat formülü | 7 çarpanlı zincir | Ortalamaya dönüşlü + bantlı | Patlamayı ve çökmeyi engellemek |
| Rakip AI | 4 fiziksel rakip | 2 soyut rakip (çıkışta) | Aylarca üretim tasarrufu |
| Bina iç mekânı | 8 | 3 tam + 5 cephe | Üretim tasarrufu |
| NPC rolü | 18 benzersiz | 8 benzersiz + 10 varyant | Üretim tasarrufu |
| Ambient yaya | Yakında 15–30 | Yakında 8–14 + ucuz canlılık katmanları | His NPC sayısından gelmiyor |
| Olay sayısı | 36 | 20 çıkışta, 36 hedef | Güncelleme yakıtı |
| Sipariş | Belirsiz | 6 kanallı kademeli sistem | Otomasyon merdiveninin omurgası |

---

## 2. Tasarım Sütunları

1. **Tahmin et, paranı bağla, sonucunu yaşa.** Kararın sonucu birkaç dakika içinde görünür.
2. **Her şey fiziksel.** Mal raf, kutu, palet ve kamyon üzerinde gerçekten hareket eder.
3. **Kural var, ama esneyebilir.** Yasal yol güvenli ve yavaş; gri yol hızlı ve tehlikeli. İkisi de geçerli oyun tarzı.
4. **Otomasyon ödüldür.** Her yeni çalışan sizi bir angaryadan kurtarır ve bunu size açıkça hissettirir.
5. **Tek geliştirici disiplini.** Tek harita, sınırlı NPC, veri odaklı sistemler, yeniden kullanılan parçalar.
6. **Sandbox önce.** Hedefler sandbox'ın üstüne biner, onun yerine geçmez.

---

## 3. Oyuncu Bakış Açısı ve Kontrol

### 3.1 Birinci şahıs kararı

Oyun **tamamen birinci şahıs**. Üçüncü şahıs kamera yok, karakter oluşturma ekranı yok, ayna yok.

Bunun getirdiği somut kazançlar:

- **Sadece el ve kol modeli gerekiyor.** Tam vücut animasyon seti üretmiyorsunuz. Bu tek kişilik bir proje için haftalarca tasarruf.
- **Yüz gerekmiyor.** Oyuncunun yüzü hiç görünmez.
- **Elde taşınan kutu ekranın alt kısmını kaplar.** Bu bir kusur değil, tasarım: taşırken önünüzü tam göremezsiniz, bu yüzden büyük kutu taşımak yavaş ve dikkatli bir iş gibi hissedilir.

### 3.2 Kamera ve kontrol ayarları

| Ayar | Değer | Not |
|---|---|---|
| FOV (görüş açısı) | 80 varsayılan, 70–100 arası ayarlanabilir | Dar FOV depoda sıkışık hissi verir |
| Yürüme hızı | 3.2 m/s | |
| Koşma | 5.0 m/s, kutu taşırken kapalı | Koşarak kutu taşınmaz |
| Kutu taşırken hız | Kutu S: %95, M: %85, L: %70 | Büyük kutu ceza getirir |
| Etkileşim mesafesi | 200 cm | |
| El | Sadece kol + eldiven modeli | Ceket rengi ayarlanabilir |

### 3.3 Temel tuşlar

| Tuş | İşlev |
|---|---|
| WASD | Hareket |
| Shift | Koşma (boş elle) |
| E | Etkileşim / **Al** (raftan, kamyon kasasından) |
| Q | Elindekini yere koy |
| F | **Ver** — elindekini/çataldakini araca veya rafa aktar |
| Tab | Şirket ekranı |
| M | Harita |
| B | Piyasa ekranı |
| N | Not defteri (tahminler ve sonuçları) |
| C | Terminal/tablet (uygun konumdaysa) |
| **E (araç yanında)** | **"Seyahat Et" — harita açılır, hedef seçilir** |

---

## 4. Ana Oyun Döngüsü

```
BİLGİ  →  TAHMİN  →  SATIN AL  →  TAŞI/DEPOLA  →  SAT/TESLİM ET  →  SONUÇ  →  YATIRIM
                                       ↑                                          |
                                       └──────────── otomasyon ────────────────────┘
```

### Adım adım

| Adım | Oyuncu ne yapar | Sistem ne yapar |
|---|---|---|
| 1. Bilgi topla | Haber ekranı, radyo, limanda dedikodu, brokerdan rapor satın alma | Sinyaller üretilir; bazıları doğru, bazıları yanıltıcı |
| 2. Tahmin et | Bir ürün seçip "bu yükselecek" der, miktar ve bütçe belirler | Tahmin kaydedilir, güven yüzdesi gösterilir |
| 3. Satın al | Kanal seçer (tezgâh / terminal / liman / gri) | Nakit veya banka düşer, tedarik süresi başlar |
| 4. Taşı ve depola | Kutuları fiziksel taşır, rafa/palete koyar | Kapasite, bozulma, eskime sayaçları işler |
| 5. Sat veya teslim et | Alıcıya götürür ya da kontrat tamamlar | Gelir gelir, itibar değişir |
| 6. Sonuç | Sonuç kartını görür | Tahmin doğru muydu, neden? Not defterine işlenir |
| 7. Yatırım | Depo/araç/personel/lisans satın alır | Kapasite ve otomasyon artar |

---

## 5. Ürün Sistemi

### 5.1 Ürün veri şeması

Her ürün satırının taşıdığı alanlar. Bu tablo doğrudan `Data/DT_Products.csv` dosyasının sütunlarıdır.

| Alan | Tip | Açıklama |
|---|---|---|
| `ID` | metin | Benzersiz kod. Örn: `S01`, `E08` |
| `NameTR` / `NameEN` | metin | Görünen ad |
| `Brand` | metin | Kurgusal marka adı. Gri mallarda boş |
| `BrandTier` | Value / Standard / Premium | Kalite kademesi |
| `Category` | metin | Staples, Food, Beverage, Household, Office, Hardware, Safety, Medical, Electronics, Components, Energy, Industrial, Event, Textile, Chemicals, Precious, Auto, Construction, Excise, Grey |
| `RiskBand` | Stable / Reactive / Volatile / Crisis | Fiyat oynaklık sınıfı |
| `BasePrice` | sayı | Baz alış fiyatı (dolar) |
| `ContainerType` | metin | BoxS / BoxM / BoxL / Pallet / Crate / ColdTote / SecureCase |
| `VolumeBU` | ondalık | Kap tipinin BU değeri (0.5 / 1 / 2 / 4 / 16) |
| `UnitsPerContainer` | sayı | Kap içine kaç adet girer |
| `WeightKg` | sayı | Elle taşıma ve araç limiti için |
| `ShelfLifeDays` | sayı | Bozulabilirler için. 0 = bozulmaz |
| `RequiresCold` | evet/hayır | Soğuk zincir zorunlu mu |
| `ObsolescencePerDay` | ondalık | Günlük değer kaybı yüzdesi. Elektronik ve moda için |
| `RequiresSecure` | evet/hayır | Kafesli/kilitli raf zorunlu mu |
| `Fragile` | evet/hayır | Sarsıntı/istif hasarına duyarlı mı |
| `LicenseRequired` | None / Standard / Restricted / Prohibited | Gri pazar sisteminin bel kemiği |
| `LicenseType` | metin | Pharma / Hazmat / Excise / Precious / Firearmless-Restricted vb. |
| `BaseDemand` | sayı | Günlük temel talep hacmi |
| `Elasticity` | ondalık | Fiyat artınca talep ne kadar düşer |
| `EventTags` | liste | Hangi olaylardan etkilenir |
| `SubstituteGroup` | metin | Yerine geçebilecek ürün grubu |
| `LaunchPhase` | EA / U1 / U2 | Çıkışta mı, güncellemede mi geliyor |

### 5.2 Kap tipleri ve BU sistemi

**BU (Box Unit) nedir?** Bu oyundaki tek hacim ölçüsü. **1 BU = 1 orta boy karton kutu.** Her şey buna çevrilir: araç kapasitesi, raf kapasitesi, elle taşıma limiti.

| Kap | BU | Fiziksel boyut | Nasıl taşınır | Tipik içerik |
|---|---|---|---|---|
| **Kutu S** | 0.5 | ~30 cm | Elle, aynı anda 2 tane | Bileşen, kartuş, ilaç, değerli maden |
| **Kutu M** | 1.0 | ~40 cm | Elle, 1 tane | Standart. Ürünlerin çoğu |
| **Kutu L** | 2.0 | ~60 cm | Elle ama yavaş | Hacimli hafif: su, kâğıt, gazoz, tekstil |
| **Palet** | 16.0 | 16 M kutu istifi | Sadece forklift/transpalet | Toptan alım, büyük sevkiyat |
| **Kasa** | 4.0 | Ahşap sandık, üstüne istiflenmez | Forklift | Jeneratör, pompa, ışık rig'i |
| **Soğuk Kap** | 1.0 | Yalıtımlı taşıma kabı | Elle, geri sayım çalışır | Donuk gıda, taze ürün, aşı |
| **Güvenli Kap** | 0.5 | Mühürlü metal kasa | Elle, kafesli rafa zorunlu | Ekran kartı, çip, altın, saat |

**Neden 7 kap, tek kap değil?** Tek tip kutu üretimi kolaylaştırır ama "küçük hacim, büyük para" gerilimini yok eder. Bu gerilim oyunun en iyi kararıdır. 7 kap ile hem üretim ucuz kalıyor (7 model, 63 ürün) hem karar zenginliği korunuyor.

### 5.3 Değer yoğunluğu — oyunun gizli ana ekseni

**Değer yoğunluğu = ürünün BU başına dolar değeri.**

| Ürün | Baz $ | BU | $/BU |
|---|---|---|---|
| Çimento Paleti | 340 | 16 | **21** |
| Su Kasası | 18 | 2 | **9** |
| Un Çuvalı | 31 | 1 | **31** |
| Alet Çantası | 95 | 1 | **95** |
| Bellek Modülü Tepsisi | 620 | 0.5 | **1.240** |
| Grafik Modülü | 1.850 | 0.5 | **3.700** |
| Külçe Altın Kutusu | 9.600 | 0.5 | **19.200** |

Bu tablo şunu üretiyor: **aynı kamyon, bin kat farklı değer taşıyabiliyor.**

| Araç | BU | Su ile maks. | Grafik modülü ile maks. | Altın ile maks. |
|---|---|---|---|---|
| V01 Kompakt Van | 32 | 288 $ | 118.400 $ | 614.400 $ |
| V02 Uzun Van | 64 | 576 $ | 236.800 $ | 1.228.800 $ |
| V04 Kapalı Kamyon | 160 | 1.440 $ | 592.000 $ | — (güvenlik limiti) |
| V05 Çekici | 320 | 2.880 $ | 1.184.000 $ | — |

Böylece "100.000 dolarımı bu tahmine yatırdım" cümlesi fiziksel olarak mümkün oluyor.

### 5.4 Marka sistemi

**Neden marka?** Üç sebeple:

1. **Hafıza.** "3 palet Norvex lazım" cümlesi "3 palet pil lazım"dan daha akılda kalır.
2. **Bedava içerik.** Aynı kutu modeli, farklı etiket = yeni ürün hissi. Model üretmeden çeşitlilik.
3. **Yeni bir karar.** Aynı ürünün ucuz / normal / pahalı markası farklı davranır.

**Kurgusal marka listesi (24 marka)**

| Marka | Kapsadığı ürünler | Kademe | Görsel kimlik |
|---|---|---|---|
| **Tapline** | Su | Value | Düz mavi, tek şerit, ucuz baskı hissi |
| **ClearSpring** | Su, meyve suyu | Standard | Açık mavi + beyaz, dağ silüeti |
| **Aquavelle** | Su, maden suyu | Premium | Lacivert + gümüş folyo hissi |
| **Golden Grain** | Un, pirinç | Standard | Buğday sarısı, klasik çuval baskısı |
| **Harvest Row** | Konserve, kiler | Standard | Toprak kırmızısı, retro etiket |
| **Sunfield** | Yağ | Standard | Sarı-yeşil, güneş ikonu |
| **Meridian Roast** | Kahve | Standard/Premium | Koyu kahve + krem, çizgisel çekirdek |
| **Cacaoline** | Kakao, çikolata | Standard | Bordo + altın çizgi |
| **Northvale** | Süt, süt ürünleri | Standard | Beyaz + soğuk yeşil |
| **Frostline** | Donuk gıda, donuk protein | Standard | Buz mavisi + kar deseni |
| **Citrix Fizz** | Gazoz | Standard | Turuncu-sarı, canlı |
| **VOLTKICK** | Enerji içeceği | Standard | Siyah + asit yeşili, agresif |
| **Purelane** | Sabun, deterjan, temizlik | Standard | Açık yeşil + beyaz |
| **Softply** | Kâğıt ürünleri | Value | Pastel, yumuşak |
| **Bureau Nine** | Ofis kağıt, kartuş | Standard | Gri + koyu mavi, kurumsal |
| **Ironcrest** | Alet, kablo, lamba | Standard/Value | Siyah + turuncu, sert |
| **Guardline** | Güvenlik ekipmanı | Standard | Sarı-siyah şerit, yansıtıcı |
| **Medrix** | Tıbbi sarf | Standard | Beyaz + yeşil haç benzeri (haç değil, artı ikonu) |
| **Vitalis Care** | Reçeteli ilaç, soğuk zincir | Premium | Beyaz + mor bant, regüle görünüm |
| **Kaido Micro** | Bütçe elektronik | Value | Gri + kırmızı, minimal Asya OEM hissi |
| **Norvex** | Bileşen (RAM, çip, SSD) | Standard | Antrasit + turkuaz devre motifi |
| **Helion Systems** | Grafik modülü, sunucu | Premium | Siyah + bakır, ağır kutu hissi |
| **Cellwright** | Telefon, router | Standard | Beyaz + açık mavi |
| **Torqline** | Jeneratör, pompa, nem alıcı | Standard | Endüstriyel gri + sarı uyarı |
| **Emberco** | Yakıt, solvent, gaz | Standard | Turuncu-kırmızı, tehlike ikonları |
| **Stagecraft** | Etkinlik ekipmanı | Standard | Siyah + magenta |
| **Palletix** | Sarf malzemeleri (kutu, bant, palet) | — | Kraft kahve + siyah stencil |
| *(markasız)* | **Gri mallar** | — | **Düz kahverengi karton + siyah stencil sayı. Etiket yok.** |

> **Görsel kural:** Yasal malın markası vardır, gri malın yoktur. Oyuncu depoda gezerken markasız kahverengi kutuları görünce ne olduğunu anlar. Bu, tek bir kural ile kurulmuş bir öğretim sistemidir.

**Kalite kademesi etkileri (sadece 8 üründe uygulanır)**

| Kademe | Alış fiyatı | Satış fiyatı | Kusur/iade oranı | Alıcı kabulü |
|---|---|---|---|---|
| Value | −25% | −15% | +8% | Premium alıcılar reddeder |
| Standard | baz | baz | baz | Herkes alır |
| Premium | +35% | +45% | −5% | Sadece premium alıcı ve etkinlik kontratları |

Kademeli 8 ürün: Su, Kahve, Konserve, Deterjan, Alet Çantası, Pil, Telefon, Enerji İçeceği.

### 5.5 Ürün kategorileri — v2'de eklenenler

v1.0'daki 50 ürüne şu kategoriler eklendi. Her biri **yeni bir karar** getirdiği için eklendi; sadece çeşitlilik olsun diye değil.

| Kategori | Getirdiği yeni karar | SKU |
|---|---|---|
| **Değerli Maden** | Kriz anında paranızı park edecek yer. Fiyatı diğer her şeyle **ters** hareket eder. Ayrıca en yüksek değer yoğunluğu | 2 |
| **Kimyasallar** | Hazmat izni zorunluluğu. İzin olmadan taşımak en ağır ceza | 3 |
| **İlaç** | Soğuk zincir + lisans + kriz talebi üçlüsü tek üründe | 1 |
| **Tekstil** | **Ölü stok** riski. Sezonu kaçırırsanız mal neredeyse satılmaz | 2 |
| **Yedek Parça** | Hem satılabilir hem kendi filonuzda kullanılabilir. İki amaçlı ilk ürün | 1 |
| **İnşaat Malzemesi** | En düşük değer yoğunluğu. Değer yoğunluğu ekseninin diğer ucu | 2 |
| **Lüks** | Sahtecilik riski + doğrulama kararı | 1 |
| **ÖTV'li Mal** | Vergi pulu mekaniği. Kaçakçılığın en klasik hâli | 1 |

**Reddedilen kategoriler ve sebepleri:**

- **Hediyelik eşya:** Düşük değer, düşük dram, yeni karar üretmiyor. X04 Manifestosuz Lot zaten "sürpriz" ihtiyacını karşılıyor.
- **Eğitim ürünleri:** Kâğıt ve kartuş zaten var; "Okul dönemi" olayı bunları zaten tetikliyor. Ayrı SKU gereksiz tekrar.

### 5.6 Sarf malzemeleri (satılmaz, kullanılır)

Bunlar ürün değil, **işletme gideridir**. Oyuncu bunları satın alır ve tüketir.

| Sarf | Ne için | Tükenirse |
|---|---|---|
| Karton kutu (S/M/L) | Dökme gelen malı paketlemek | Mal paketlenemez, satılamaz |
| Ambalaj bandı | Kutu kapatmak | Kutu açılır, hasar riski |
| Streç film | Palet sarmak | Palet taşınırken dağılır |
| Ahşap palet | İstifleme | Toptan alım yapılamaz |
| Yakıt | Araç seyahati | Seyahat edilemez |
| Soğutma elektriği | Soğuk bölge | Soğuk zincir kırılır |
| Etiket/barkod | Kayıtlı satış | Beyaz işlem yapılamaz (!) |

> **Tasarım notu:** "Etiket/barkod" sarf malzemesi gri pazara ince bir bağ kuruyor. Etiketiniz bittiğinde ya beklersiniz ya da kayıtsız satarsınız.

---

## 6. Sipariş ve Tedarik Sistemi

Bu bölüm "ürünü nereden alıyorum?" sorusunun cevabıdır. Cevap: **başta gidersiniz, sonra bilgisayardan sipariş verirsiniz.** Bu kademelenme oyunun otomasyon ödül merdiveninin ilk basamağıdır.

### 6.1 Altı kanal

| # | Kanal | Nasıl çalışır | Ne zaman açılır | Fiyat farkı | Teslim süresi | Gri mal? |
|---|---|---|---|---|---|---|
| 1 | **Tezgâh** (yüz yüze) | Tedarikçiye gidersiniz, NPC ile konuşursunuz, kutuları kendiniz taşırsınız | Başlangıç | **Baz (en ucuz)** | Anında, siz taşırsınız | Hayır |
| 2 | **Depo Terminali** | Ofisteki masaüstü bilgisayara oturursunuz | Tedarikçiyi **bir kez** ziyaret ettikten sonra | +6% | 4–12 saat, rampaya gelir | Hayır |
| 3 | **Saha Tableti** | Her yerden sipariş verirsiniz | Şirket değeri 75.000 $ | +12% | 6–18 saat | Hayır |
| 4 | **Liman İthalatı** | Konteyner sipariş edersiniz, gümrükten geçer | Genel Ticaret Lisansı sonrası | **−20%** | 24–72 saat + gecikme riski | Manifestosuz lot burada |
| 5 | **Acil Tedarikçi** | Telefonla anında | Her zaman | **+60–120%** | 1–2 saat | Hayır |
| 6 | **Gri Kanal** | **Sadece fiziksel. Gece. Belirli noktalarda. Nakit.** | Bölüm hedefi 2 sonrası | −15 ile −40% | Anında | **Sadece burada** |

### 6.2 Neden bu tasarım doğru

- **Tezgâhın en ucuz olması şart.** Yoksa oyuncu hiç dışarı çıkmaz ve şehir boşa üretilmiş olur. Fiziksel gitmek zaman maliyeti; karşılığında para indirimi alırsınız. Gerçek bir takas.
- **Terminal bir ödüldür.** İlk açıldığında oyuncu "artık her sabah limana gitmeyeceğim" der. Bu duygu, otomasyon merdiveninin ilk zaferidir.
- **Her tedarikçiyi bir kez ziyaret etme şartı** haritayı sürekli anlamlı tutar. Yeni tedarikçi = yeni gezi.
- **Gri kanalın asla terminalde olmaması** en önemli kural. Gri iş yapmak için fiziksel olarak riskli bir yere, gece, nakitle gitmek zorundasınız. Bu his, bir menü tıklamasıyla asla kurulamaz.

### 6.3 Nakit ve Banka ayrımı

| | Banka | Nakit |
|---|---|---|
| Beyaz alım-satım | Evet | Evet |
| Gri alım-satım | **Hayır** | **Evet, zorunlu** |
| Denetimde görünür mü | Evet, tam kayıt | Depoda bulunursa **kanıt** |
| Faiz / kredi | Evet | Hayır |
| Taşıma limiti | Sınırsız | Kasada max. limit; fazlası şüphe yaratır |

**Banka → Nakit çekim:** Günlük eşiğin (10.000 $) üstündeki çekimler `Şüphe` puanı ekler. Bu, gri pazarın gizli maliyetidir.

---

## 7. GRİ PAZAR VE DEVLET DENETİMİ SİSTEMİ

Bu, v2.0'ın en büyük eklemesi ve oyunun farklılaştırıcısıdır.

### 7.1 Temel fikir — İki Defter

Şirketiniz kurumsaldır, dolayısıyla kayıt tutmak zorundadır. Sistem iki sayıyı ayrı ayrı takip eder:

- **Fiziksel Stok:** Depoda gerçekten duran mal.
- **Kayıtlı Stok:** Resmî defterde görünen mal.

Normalde bu ikisi eşittir. Ama:

| İşlem | Fiziksel stok | Kayıtlı stok | Sonuç |
|---|---|---|---|
| Faturalı al | ↑ | ↑ | Fark yok. Güvenli |
| Faturalı sat | ↓ | ↓ | Fark yok. Güvenli |
| **Faturasız al** | ↑ | değişmez | **Fark: +** (açıklanamayan mal) |
| **Faturasız sat** | ↓ | değişmez | **Fark: −** (kaybolmuş mal) |

**Stok Farkı = Fiziksel − Kayıtlı**

Denetim tam olarak buna bakar.

### 7.2 Bu sistemin güzel tarafı

**Gri alıp gri satarsanız fark sıfırlanır.**

Yani "temiz" gri strateji şudur: kayıtsız al, kayıtsız sat, defterlerine hiç dokunma. Yüksek marj, sıfır iz.

**Ama:** gri alıcılar daha az öder ve sınırlı miktar alır. Büyük para beyaz alıcıda (kontratlar, perakende zinciri). Yani asıl kazanç için **gri alıp beyaz satmanız** gerekir — ve işte o zaman elinizde nereden geldiğini açıklayamadığınız mal olur.

Bu, tek bir sayı etrafında kurulmuş gerçek bir strateji bulmacasıdır.

### 7.3 Lisans sistemi

Her ürünün bir `LicenseRequired` değeri vardır:

| Seviye | Anlamı | Örnek ürünler |
|---|---|---|
| **None** | Serbest ticaret | Su, pirinç, kâğıt, çimento |
| **Standard** | Genel Ticaret Lisansı yeterli | Gıda, elektronik, alet, tekstil |
| **Restricted** | Ürüne özel izin gerekir | İlaç, kimyasal, değerli maden, ÖTV'li mal, lüks |
| **Prohibited** | Yasal izin yok. Sadece gri kanal | Manifestosuz lot, çalıntı bileşen |

**Alınabilecek izinler**

| İzin | Maliyet | Şart | Açtığı ürünler |
|---|---|---|---|
| Genel Ticaret Lisansı | 2.500 $ | Yok (başlangıç) | Standard tüm mallar |
| İthalat Lisansı | 8.000 $ | 30 gün faaliyet | Liman kanalı |
| Hazmat İzni | 15.000 $ | Temiz denetim geçmişi | Solvent, gaz, yakıt, gübre |
| Farmasötik İzni | 22.000 $ | İtibar 60+ | Reçeteli ilaç, aşı taşıyıcı |
| ÖTV Ruhsatı | 18.000 $ | Temiz sicil | Vergi pullu mallar |
| Değerli Maden Yetkisi | 30.000 $ | Şirket değeri 250.000 $+ | Altın, gümüş, lüks saat |

**İzinler askıya alınabilir.** Bir ihlalde 7 gün, ikincisinde 21 gün, üçüncüsünde kalıcı olarak iptal edilir. İptal sonrası yeniden alma bedeli iki katıdır.

### 7.4 Şüphe (Heat) — 0 ile 100 arası

Ekranda küçük bir gösterge olarak durur. Denetim sıklığını ve sertliğini belirler.

**Şüpheyi artıranlar**

| Eylem | Artış |
|---|---|
| Gri alım | İşlem değerinin 100.000 $ başına +4 |
| Gri satış | İşlem değerinin 100.000 $ başına +6 |
| İzinsiz kısıtlı mal satışı | +15 |
| Yasak mal bulundurma (gün başına) | +2 |
| Eşik üstü nakit çekimi | +3 |
| Kriz sırasında fiyat fahişliği | +8 ile +20 (paya ve fiyata göre) |
| Denetimde tutarsız beyan | +25 |
| Rakip ihbarı | +10 (rastgele tetikli) |
| Yol kontrolünde kaçmak | +30 |

**Şüpheyi düşürenler**

| Eylem | Düşüş |
|---|---|
| Zaman (doğal sönüm) | Günde −1.5 |
| Temiz denetim geçmek | −12 |
| Compliance Clerk (uyum sorumlusu) çalışan | Günde ek −1.0 |
| Kamu/belediye kontratını zamanında tamamlamak | −8 |
| Etkinlik sponsorluğu (itibar aklama) | −10, bedeli 15.000 $ |
| "Danışmanlık ücreti" (rüşvet) | −25, ama %18 ihtimalle **ters teper**: +40 ve anında nokta denetimi |

**Şüphe bantları**

| Şüphe | Durum | Sonuç |
|---|---|---|
| 0–20 | Temiz | Sadece rutin denetim |
| 21–45 | İzlemede | Rutin denetim sıklığı 2 katı |
| 46–70 | Şüpheli | Nokta denetimi mümkün; yol kontrolü %15 |
| 71–90 | Hedefte | Nokta denetimi sık; liman kontrolü zorunlu |
| 91–100 | Soruşturma | Vergi müfettişi devrede; faaliyet durdurma riski |

### 7.5 Dört denetim türü

| Tür | Kim yapar | Ne zaman | Neye bakar | Uyarı |
|---|---|---|---|---|
| **Rutin Denetim** | Şehir Denetçisi (N17) | Her 10–14 günde bir | Rastgele 3 ürünün sayımı | **24 saat önce bildirim** |
| **Nokta Denetimi** | Şehir Denetçisi + ekip | Şüphe 46+ olduğunda rastgele | **Tüm deponun sayımı**, gizli alanlar dahil | **Yok. Kapıda.** |
| **Liman Kontrolü** | Gümrük Memuru | Her ithalatta, şüpheye bağlı olasılıkla | Konteyner içeriği vs. manifesto | Gümrük hattında |
| **Vergi Denetimi** | Vergi Müfettişi | 90 günde bir + şüphe 91+ | **Depoya bakmaz.** Beyan edilen kâr ile şirket büyüklüğünü karşılaştırır | 3 gün önce |

**Yol kontrolü** ayrıca bir *seyahat olayı* olarak gerçekleşir — bkz. Bölüm 9.3.

### 7.6 Yakalanma sonuçları (kademeli)

| Kademe | Sonuç | Etki |
|---|---|---|
| 1 | Uyarı | Kayda geçer, maliyet yok |
| 2 | Para cezası | Fark değerinin %40–120'si |
| 3 | **Mala el koyma** | Mal fiziksel olarak depodan alınır. Ekranınızın önünde forkliftle götürülür |
| 4 | Lisans askıya alma | O kategoride 7–21 gün ticaret yok |
| 5 | Faaliyet durdurma | 3 gün hiçbir ticaret yok. Kontratlar patlar |
| 6 | Kapatma | Sadece tekrarlanan azami ihlallerde. **Bir kez kurtarma penceresi verilir** |

> **Tasarım notu:** El koyma kademesi bilerek *görsel* tasarlandı. Denetçiler gelir, kutularınızı sayar, forkliftle yükler ve götürür. Sayı ekranında eksilmez — deponuz gözünüzün önünde boşalır. Bu, yayıncı için en güçlü an ve oyuncu için en kalıcı ders.

### 7.7 Gizleme araçları

Gri oynamak isteyen oyuncunun elindeki karşı hamleler:

| Araç | Maliyet | Ne yapar | Zayıflığı |
|---|---|---|---|
| **Güvenli Oda** (depo yükseltmesi) | 45.000 $ | 40 BU mal rutin denetimde sayılmaz | Nokta denetiminde bulunur |
| **Sahte irsaliye** (brokerdan) | Mal değerinin %12'si | X BU gri malı kayıtlı hâle getirir | %10 ihtimalle sahte olduğu anlaşılır: +40 Şüphe |
| **Liman kiralık konteyner** | Aylık 3.500 $ | Depo dışı saklama; hiç denetlenmez | Taşıma süresi + hırsızlık riski + acil satışta ulaşılamaz |
| **Gece sevkiyatı** | Zaman | 22:00–05:00 arası gri işlemler yarı şüphe üretir | Gece çalışan personel %50 zamlı |
| **Uyum Sorumlusu** (personel) | 4.500 $ + 1.900 $/ay | Şüphe sönümü hızlanır, ceza %35 azalır | Maaş gideri |
| **Karışık yükleme** | Ücretsiz | Gri malı beyaz malın arkasına istiflemek yol kontrolünde %35 koruma sağlar | Nokta denetiminde işe yaramaz |

### 7.8 Stokçuluk ve fiyat fahişliği (sizin fikriniz, sisteme oturtulmuş hâli)

Depolayıp kıtlık yaratıp fiyat belirlemek **meşru bir strateji olarak kalır**, ama bedeli vardır.

Sistem şunu takip eder: `PazarPayı` = sizdeki stok / şehirdeki toplam arz (ürün başına).

| Koşul | Sonuç |
|---|---|
| Pazar payı > %60 **ve** fiyatınız baz fiyatın 1.8 katından yüksek | "Fiyat fahişliği" bayrağı açılır |
| Bayrak açıkken her gün | +8 Şüphe |
| Ürün **temel ihtiyaç** ise (su, gıda, ilaç, maske) | +20 Şüphe, ceza iki katı |
| Muhabir NPC (N16) haber yapar | Şehir çapında itibar −15, rakipler uyarılır ve karşı stok yapar |
| Belediye "azami fiyat" kararı çıkarabilir | 3 gün boyunca o ürüne fiyat tavanı. Elinizdeki stok kilitlenir |

**Karşı hamle:** Aynı üründen bir kısmını belediye kontratına makul fiyattan verirsiniz. Kâr düşer ama bayrak kapanır. Bu, gerçek bir "ne kadar açgözlü olmalıyım" kararıdır.

### 7.9 Devlet görevlisi NPC'leri

| ID | Rol | Davranış | Rüşvet alır mı |
|---|---|---|---|
| **G01** | **Şehir Denetçisi** | Bürokratik, öngörülebilir. 24 saat önceden haber verir. Kurallara harfiyen uyar | **Hayır.** Ama itibarla yumuşar |
| **G02** | **Gümrük Memuru** | Limanda. Konteyner açar. İşlem hızlandırma ücreti alır (yasal) ya da "öncelik ücreti" (yasadışı) | **Evet**, riskli |
| **G03** | **Vergi Müfettişi** | Depoya hiç gelmez. Rakamlara bakar. En tehlikeli tip, çünkü saklamak işe yaramaz | **Hayır** |
| **G04** | **Yol Kontrol Ekibi** | Seyahat sırasında rastgele çıkar. Yükünüzü irsaliyeyle karşılaştırır | **Evet**, ucuz ama sık ters teper |

### 7.10 Oyuncuya nasıl öğretilir

Gri sistem birden açılmaz. Kademeli tanıtım:

| Aşama | Ne olur |
|---|---|
| İlk 30 dakika | Gri diye bir şey yok. Sadece al-sat öğrenilir |
| Hedef 1 sonu | İlk rutin denetim gelir. Temiz olduğunuz için kolayca geçersiniz. Sistem tanıtılmış olur |
| Hedef 2 | Limanda bir NPC size "faturasız daha ucuza verebilirim" der. **İlk gri teklif.** Reddedebilirsiniz |
| Hedef 2 sonu | Küçük bir gri işlem yaparsanız Şüphe göstergesi ilk kez ekranda belirir |
| Hedef 3 | İlk lisanslı ürün fırsatı çıkar; izin almadan satarsanız ilk ceza |
| Hedef 4+ | Tüm araçlar açık. Oyuncu kendi tarzını seçer |

**Kritik kural:** Oyun asla "gri oyna" demez. Gri oynamak %100 opsiyoneldir ve tamamen beyaz oynayarak oyunu bitirmek mümkündür — sadece daha yavaş.

---

## 8. Ekonomi Modeli

### 8.1 Fiyat hesabı (v1'deki formül değişti)

**v1 sorunu:** `P = Base × Trend × Shock × Season × Rival × Scarcity × PlayerPressure` — yedi çarpanın hepsi 1.3 olursa fiyat 6.3 katına çıkar. Kontrol edilemez.

**v2 çözümü:** Ortalamaya dönüşlü fiyat + olay darbeleri + sert bantlar.

Basit dille:
1. Her ürünün bir "normal" fiyatı vardır.
2. Fiyat rastgele yukarı aşağı gezinir ama **her gün biraz normale doğru çekilir**.
3. Bir olay olduğunda fiyata **tek seferlik bir itki** verilir.
4. Sonuç her zaman ürünün risk bandındaki sert sınırlar içinde tutulur.

```
YeniFiyat = EskiFiyat
          + Çekim × (NormalFiyat - EskiFiyat)      ← ortalamaya dönüş
          + OlayDarbesi                             ← olaylar
          + Gürültü                                 ← küçük rastgelelik
          + OyuncuBaskısı                           ← sizin pazar payınız
Sonuç, risk bandının tavan/taban değerlerine kırpılır.
```

**Risk bantları ve sert sınırlar**

| Band | Günlük oynaklık | Alt sınır | Üst sınır | Çekim gücü |
|---|---|---|---|---|
| Stable | ±%3 | 0.85 × baz | 1.20 × baz | Güçlü (0.25) |
| Reactive | ±%7 | 0.70 × baz | 1.50 × baz | Orta (0.15) |
| Volatile | ±%14 | 0.50 × baz | 2.20 × baz | Zayıf (0.08) |
| Crisis | ±%25 | 0.40 × baz | **6.00 × baz** | Çok zayıf (0.04) |

Böylece: ekran kartı krizde 6 katına çıkabilir, su asla çıkamaz. Hem inandırıcı hem dengelenebilir.

> **Uygulama kuralı:** Bu modeli önce Excel'de kurun. 200 günlük simülasyonu 20 farklı tohumla çalıştırın. Hiçbir ürün her seferinde kazanan olmamalı. Unreal'e ekonomi kodu yazmadan önce bu testi geçin.

### 8.2 Diğer formüller

| Ne | Hesap |
|---|---|
| **Talep** | `Talep = TemelTalep × Olay × İtibar × (Fiyat/BazFiyat)^(-Elastikiyet)` |
| **Beklenen kâr** | `(BeklenenSatış − BirimMaliyet) × Adet − Lojistik − İşçilik − Ücretler − Vergi` |
| **Maruziyet (risk tutarı)** | `Stok değeri + Kontrat ceza riski + Vadesi gelen borç + El koyma riski` |
| **Şirket değeri** | `Nakit + Banka + Stok satış değeri + Ekipman − Borç − Bekleyen cezalar` |
| **El koyma riski** | `|StokFarkı| × BirimDeğer × ŞüpheÇarpanı` |

### 8.3 Finans

Basitleştirildi: 5 kredi ürünü yerine 2.

| Ürün | Limit | Faiz | Kullanım |
|---|---|---|---|
| **Kredi Limiti** | Şirket değerinin %40'ı, max 250.000 $ | Değişken, günlük | Normal büyüme ve nakit akışı |
| **Acil Kurtarma Kredisi** | 100.000 $ | Çok yüksek sabit | Sadece batmak üzereyken açılır |

**İflas:** Nakit + Banka < 0 **ve** vadesi gelen borç ödenemiyorsa bir **kurtarma penceresi** açılır: 48 saat içinde varlık satarak veya acil kredi alarak kurtulabilirsiniz. Aksi hâlde oyun biter.

---

## 9. Lojistik ve Seyahat Sistemi

### 9.1 Manuel araç kullanımı yok

**Karar:** Oyuncu hiçbir zaman araç sürmez.

**Neden:** Sürülebilir araç, tek kişilik projelerde neredeyse her zaman planlanandan üç kat pahalıdır — araç fiziği ayarı, çarpışma, hasar, park, trafik yapay zekâsı, kamera, ses katmanları, takılma hataları. Ve oyuncu için beşinci saatten sonra angarya hâline gelir.

**Bu kararın büyük yan kazancı:** Şehrin yol ağının sürülebilir olması gerekmiyor. Trafik yapay zekâsı, araç çarpışması, yol navigasyonu ve şerit sistemi tamamen iptal. Bu, aylarca üretim tasarrufu demek.

### 9.2 "Seyahat Et" akışı

1. Deponuzda kutuları araca yüklersiniz (fiziksel, bkz. Bölüm 10).
2. Aracın sürücü kapısına yaklaşırsınız → ekranda **"E — Seyahat Et"** görünür.
3. E'ye basınca harita açılır. Ulaşılabilir noktalar işaretlidir; her birinde **mesafe, süre, yakıt maliyeti** yazar.
4. Hedefi seçersiniz.
5. **2–3 saniyelik geçiş:** Kamera araç kabininin içine geçer, ön cam bulanıklaşır, motor sesi + kısa bir yol ambiyansı çalar, oyun saati ilerler.
6. Hedefte aracın yanında belirirsiniz. Yükünüz araçtadır.

**Geçiş neden 2–3 saniye ve neden kabin içinden?** Ekran karartıp yüklemek "menü" hissi verir. Kabin içinden kısa bir geçiş, aynı maliyetle "yolculuk yaptım" hissini korur.

### 9.3 Seyahat olayları — sürüşün yerini alan dram

Seyahat sırasında %20 ihtimalle bir olay çıkar ve **oyun durur, karar sorulur**. Bu, manuel sürüşün sağlayacağı gerilimin daha ucuz ve daha yoğun hâlidir.

| Olay | Seçenekler | Sonuç |
|---|---|---|
| **Yol kontrol noktası** | Uzun yoldan git / Geri dön / Riske gir | Uzun yol: +%40 süre, +yakıt. Riske gir: yük denetlenir |
| **Trafik tıkanıklığı** | Bekle / Ara sokaklardan git | Bekle: +süre. Ara sokak: +%15 hasar riski (Fragile mallar) |
| **Yol çalışması** | Bekle / Geri dön | Kontrat süresi baskısı |
| **Kaza görgü tanıklığı** | Yardım et / Devam et | Yardım: +itibar, −süre. Devam: küçük itibar kaybı |
| **Rakip kamyonu** | Takip et / Yoluna bak | Takip: rakibin nereye mal götürdüğünü öğrenirsiniz (+bilgi) |
| **Araç arızası** | Tamir çağır / Yavaş devam | Tamir: para + süre. Yavaş: teslimat riski |
| **Şüpheli teklif** | Dur ve konuş / Geç | Gri tedarikçi tanışması |

Yol kontrol noktasında **"Riske gir"** seçilirse:

```
Yakalanma ihtimali = 15% temel
  + Şüphe/4                          (Şüphe 60 ise +15%)
  + gri mal BU oranı × 20%
  − karışık yükleme yaptıysanız 35% (çarpan)
  − sahte irsaliyeniz varsa 50% (çarpan)
```

### 9.4 Rota maliyeti

Seyahat, ekonomik olarak hâlâ tam simüle edilir. Sadece direksiyon yok.

| Faktör | Etki |
|---|---|
| Mesafe | Temel süre ve yakıt |
| Araç sınıfı | Hız, kapasite, yakıt tüketimi |
| Yük ağırlığı | %10'a kadar ek yakıt |
| Trafik (saat) | 07–09 ve 17–19 arası +%35 süre |
| Yol kapanması (olay) | Alternatif rota + maliyet |
| Hava | Yağmurda +%15 süre |
| Sürücü becerisi (personel) | Süre ve arıza riski azalır |

### 9.5 Araç listesi

| ID | İsim | Kapasite (BU) | Hız | Özellik |
|---|---|---|---|---|
| V01 | Mulebox Kompakt Van | 32 | Yavaş | Ucuz, başlangıç aracı |
| V02 | Mulebox Uzun Van | 64 | Orta | Genel amaçlı |
| V03 | Dockrunner Açık Kasa | 96 | Orta | Ucuz hacim, **güvenlik yok** (Güvenli Kap taşınamaz) |
| V04 | Dockrunner Kapalı Kamyon | 160 | Orta | Kontrat aracı, kilitli kasa |
| V05 | Harborline Çekici | 320 | Hızlı | Endgame, sadece büyük rampalara yanaşır |
| V06 | Harborline Soğutmalı | 128 | Orta | Sadece Soğuk Kap. Elektrik tüketir |
| V07 | RapidCart Pikap | 32 | Çok hızlı | Acil teslimat, düşük yakıt |
| V08 | Forklift FL-2 | — | — | Sadece depo içi. Palet ve Kasa taşır |

---

## 10. Fiziksel Taşıma ve Yükleme

Bu bölüm "kutular araca nasıl yüklenecek?" sorusunun cevabıdır.

### 10.1 Karar: Kutular kaybolmaz, görünür şekilde istiflenir

Kutu araca girdiğinde **yok olmaz**. Aracın kasasında görünür bir istif oluşur. Ama bu istif fizik motoruyla simüle edilmez.

**Nasıl çalışır (teknik olarak basit hâli):**

1. Her aracın kasasında önceden tanımlı **yuva noktaları** vardır (V01 için 32 nokta, V02 için 64...).
2. Elinizde kutuyla kasanın arkasındaki **tetikleme alanına** girer, F tuşuna basarsınız.
3. Kutu elinizden çıkar, **yarım saniyede** boş olan ilk yuvaya yumuşakça süzülür ve orada durur.
4. O andan itibaren kutu artık ayrı bir nesne değil, aracın kargo listesine eklenmiş bir **çoğaltılmış model** (Instanced Static Mesh) olur.

**Neden bu yöntem:**

- Oyuncu kamyonun dolduğunu **görür**. Bu, bu tür oyunlardaki en tatmin edici görsel geri bildirim.
- Ama 320 ayrı kutu için 320 ayrı fizik nesnesi yok. Tek çizim yükü. Performans sorunu çıkmaz.
- Kutunun havada zıplaması, birbirine takılması, kamyondan düşmesi gibi hata kaynakları hiç oluşmaz.

**Ne yapılmayacak:** Gerçek fizikli istifleme. Kutuların birbirine yaslanıp devrilmesi eğlenceli görünür ama tek kişilik bir projede sonu gelmeyen hata kaynağıdır.

### 10.2 Paletler

Palet, 16 kutuluk tek bir modeldir. Forklift ile alınır, kamyona tek hamlede girer. 16 ayrı kutu asla oluşturulmaz.

Depoda bir paletten tek kutu almak isterseniz: paleti "aç" komutuyla 16 kutuya ayırırsınız. O anda 16 gerçek kutu oluşur. Kapatırsanız tekrar tek modele döner.

### 10.3 Otomasyon merdiveni — kutuları kim taşıyor?

Bu, oyunun en önemli ödül sistemi. Her basamak sizi bir angaryadan kurtarır ve oyun bunu açıkça kutlar.

| Basamak | Açılış şartı | Ne devredilir | Oyuncunun kazandığı his |
|---|---|---|---|
| **0. Tek başına** | Başlangıç | Hiçbir şey. Her kutuyu siz taşırsınız | Emek |
| **1. Mal Kabul** | İlk Depo Görevlisi (1.800 $) | Gelen sevkiyat rampada otomatik indirilir | "Artık kamyonu boşaltmıyorum" |
| **2. Raflama** | İkinci Depo Görevlisi | Rampadaki mal rafa dizilir | "Depom kendini topluyor" |
| **3. Paketleme** | Paketleme Masası yükseltmesi | Dökme mal otomatik kutulanır | "Bant çekmeyi bıraktım" |
| **4. Yükleme** | Forklift Operatörü (4.200 $) | Araç otomatik yüklenir | "Sadece nereye gideceğini söylüyorum" |
| **5. Seyahat** | Şoför (3.500 $) | Aracı şoför götürür, siz depoda kalırsınız | **"Artık işi ben yapmıyorum, yönetiyorum"** |
| **6. Satın alma** | Satın Alma Sorumlusu (8.000 $) | Belirlediğiniz kurallara göre otomatik sipariş | "Şirketim kendi kendine nefes alıyor" |
| **7. Tahmin desteği** | Piyasa Analisti (12.000 $) | Sinyal gürültüsü azalır, ön uyarı gelir | "Geleceği daha net görüyorum" |

> **Tasarım kuralı:** Her basamak açıldığında ekranda kısa bir kutlama görünür ve **kaç saat kazandığınız** yazılır. Örn: "Forklift Operatörü işe alındı — günde ~11 dakika kazandınız."

**5. basamak özellikle önemli:** Şoför tuttuğunuzda artık siz seyahat etmezsiniz. Rota gönderirsiniz, sonuç raporu gelir. Ama isterseniz **hâlâ kendiniz gidebilirsiniz** — çünkü gri işler için gitmek zorundasınız. Şoför gri mal taşımaz.

### 10.4 Personel listesi

| Rol | İşe alım | Aylık | Görevi |
|---|---|---|---|
| Depo Görevlisi | 1.800 $ | 950 $ | Mal kabul, raflama |
| Forklift Operatörü | 4.200 $ | 1.700 $ | Palet, yükleme |
| Şoför | 3.500 $ | 1.500 $ | Rota, teslimat |
| Kıdemli Şoför | 7.000 $ | 2.600 $ | Uzun rota, arıza riski −%18 |
| Satın Alma Sorumlusu | 8.000 $ | 3.200 $ | Otomatik sipariş, −%3 alış |
| Piyasa Analisti | 12.000 $ | 4.200 $ | Tahmin gürültüsü −%8 |
| Uyum Sorumlusu | 4.500 $ | 1.900 $ | Şüphe sönümü, ceza −%35 |
| Tamirci | 5.000 $ | 2.100 $ | Araç bakımı −%20 |

Kadro **0–8 kişi** aralığında tutulur. 20 kişilik ekip yönetimi yoktur.

---

## 11. Depo Sistemi

| Depolama sınıfı | Kapasite kuralı | Neyi alır |
|---|---|---|
| **Raf** | Raf başına 12 BU | Kutu S/M/L |
| **Palet alanı** | Alan başına 16 BU | Palet, Kasa |
| **Soğuk bölge** | Bölge başına 24 BU | Soğuk Kap. Elektrik tüketir |
| **Kafesli/Güvenli raf** | Kafes başına 8 BU | Güvenli Kap. Hırsızlığa karşı |
| **Güvenli Oda** (yükseltme) | 40 BU | Rutin denetimde sayılmaz |
| **Zemin** | Sınırsız ama yürüme yolunu kapatır | Her şey. Ceza: yavaş hareket, denetimde eksi puan |

**Depo yükseltmeleri**

| Yükseltme | Etki |
|---|---|
| Raf Genişletme I/II/III | +8 / +18 / +32 raf |
| Soğuk Bölge I/II | +1 / +2 soğuk bölge |
| Kafesli Raf | Güvenli Kap depolama açılır |
| Güvenli Oda | Gizli 40 BU |
| Hızlı Rampa | Mal kabul süresi −%30 |
| Paketleme Masası | Otomatik kutulama |
| İkinci Rampa | Aynı anda 2 araç yüklenir |
| Güvenlik Sistemi | Hırsızlık −%70 |

---

## 12. Sandbox ve Hedef Zinciri

**Karar:** Kapalı kampanya yok. Sandbox var, üstünde hedef zinciri var.

**Fark nedir?**
- Kampanya: her bölüm ayrı içerik, ayrı sahne, ayrı harita durumu. Çok pahalı.
- Hedef zinciri: aynı sandbox dünyada, sırayla açılan hedefler. Yeni sistemler hedeflerle açılır. Çok ucuz, aynı ilerleme hissi.

| Hedef | Açtığı sistem | Hedef | Tahmini süre |
|---|---|---|---|
| **H1 — İlk Yük** | Al-sat, elle taşıma, tezgâh kanalı | 25.000 $ şirket değeri | 1.5–2 sa |
| **H2 — Raf Baskısı** | Depolama, bozulma, terminal kanalı, ilk denetim | 60.000 $ | 2–2.5 sa |
| **H3 — Şehir Sinyalleri** | Haberler, tahmin, güven göstergesi | 3 başarılı tahmin | 2.5 sa |
| **H4 — Sözleşme Penceresi** | Kontratlar, ceza, teslimat riski | 3 premium kontrat | 2.5 sa |
| **H5 — Gri Hat** | **Gri kanal, lisanslar, şüphe, denetimler** | 100.000 $ (yolu serbest) | 3 sa |
| **H6 — Rekabet** | Rakipler, ihale, tedarikçi kilitleme | 2 pazarda öne geç | 3 sa |
| **H7 — Şok Haftası** | Zincirleme kriz olayları | Krizde pozitif nakit | 3 sa |
| **H8 — Büyük Bahis** | Yüksek kaldıraç, bölgesel dağıtım | 500.000 $ | 3–4 sa |
| **H9 — Deadline** | Final mega kontrat + piyasa şoku | Kontratı tamamla | 3–4 sa |

**Çıkışta (Early Access):** H1–H5 tam çalışır. H6–H9 güncellemelerle gelir.
**Sandbox modu:** Hedeflerden bağımsız, sınırsız oynanır. İlk günden açık.

---

## 13. Olaylar

Çıkışta 20 olay, hedef 36. Aşağıda çıkış seti.

| ID | Olay | Etki | Süre |
|---|---|---|---|
| E01 | Liman Gecikmesi | İthal arz −%20 | 2–4 gün |
| E02 | Sıcak Dalgası | İçecek/vantilatör talebi ↑ | 2 gün |
| E03 | Soğuk Dalgası | Isıtıcı/enerji talebi ↑ | 2 gün |
| E04 | Elektrik Kesintisi | Jeneratör/pil fiyat sıçraması | 1–2 gün |
| E05 | Fırtına Uyarısı | Tıbbi/alet talebi ↑ | 2 gün |
| E06 | Sel | Pompa/nem alıcı patlar | 2 gün |
| E07 | Festival İlanı | Etkinlik talebi +%60 | 3–5 gün |
| E08 | Konteyner Denetimi | Liman kapasitesi −%35, **denetim ihtimali ×2** | 2 gün |
| E09 | Yol Kapanması | Rota maliyeti +%25 | 1–2 gün |
| E10 | Tedarikçi İndirimi | Alış fiyatı −%12 | 1 gün |
| E11 | Tedarikçi Kıtlığı | Alım tavanı −%40 | 2 gün |
| E12 | **Çip Krizi** | Bileşen fiyatları +%80–250 | 4–7 gün |
| E13 | **Yapay Zekâ Dalgası** | Grafik modülü ve bellek talebi patlar | 5–9 gün |
| E14 | İnşaat Patlaması | Alet/güvenlik/çimento talebi ↑ | 3 gün |
| E15 | Salgın Uyarısı | Maske/eldiven/ilaç kriz seviyesi | 3–6 gün |
| E16 | Yakıt Zammı | Tüm rota maliyeti +%25 | 3 gün |
| E17 | Kur Dalgalanması | İthal fiyatlar ±%8 | 2 gün |
| E18 | **Denetim Kampanyası** | 5 gün boyunca tüm denetim ihtimalleri ×2 | 5 gün |
| E19 | Panik Alım | Temel ihtiyaç talebi +%25 | 1 gün |
| E20 | Piyasa Sakinliği | Tüm oynaklık azalır | 2 gün |

Güncellemelerle gelecek 16 olay v1.0 belgesindeki havuzdan seçilecektir.

---

## 14. Kontratlar

Çıkışta 10 şablon (v1'de 20 vardı).

| Kontrat | Hacim | Süre | Risk | Not |
|---|---|---|---|---|
| Yerel İkmal | 50–150 | 6–12 sa | Düşük | Öğretici kontrat |
| Kafe Haftalık | 80–250 | 12–24 sa | Düşük | Düzenli gelir |
| Ofis Yenileme | 100–300 | 18–36 sa | Düşük | |
| Etkinlik İçecek | 300–900 | 12–24 sa | Orta | Festival olayına bağlı |
| Depo İkmali | 500–1500 | 24–48 sa | Orta | Hacim testi |
| Acil Su | 400–1200 | 6–10 sa | Yüksek | Kriz kontratı |
| Tıbbi Stok | 200–700 | 12–24 sa | Yüksek | **Lisans gerekir** |
| İnşaat Partisi | 200–600 | 18–36 sa | Orta | |
| **Belediye İhalesi** | 600–2000 | 24–48 sa | Orta | **Şüphe düşürür**, itibar aklar |
| **Deadline Mega Kontrat** | 2000–5000 | 24–36 sa | Aşırı | Final |

---

## 15. Rakipler

Çıkışta 2 rakip. **Fiziksel olarak simüle edilmezler** — sokakta kamyonlarını görmezsiniz. Varlıkları haber, fiyat ve stok üzerinden hissedilir.

| Rakip | Kişilik | Strateji | Zayıflığı |
|---|---|---|---|
| **Northforge Supply** | Agresif | Erken stoklar, düşük marjla satar, sizi fiyatta boğar | Likidite. Kriz uzarsa nakitsiz kalır |
| **Bluegate Trading** | Analitik | Bilgi satın alır, az ama doğru hamle yapar | Yavaş. Ani olaylara geç tepki verir |

**Rakip nasıl görünür:**
- Haber: "Northforge, şehirdeki jeneratör stoğunun yarısını aldı."
- Fiyat: Aldıkları ürünün arzı düşer, fiyat yükselir.
- Tezgâh: Gittiğiniz tedarikçide "üzgünüm, hepsi satıldı" cevabı.
- İhale: Kontrata rakip teklif verirler.
- **İhbar:** Şüpheniz yüksekse, sizi ihbar edebilirler.

Güncellemelerle 2 rakip daha eklenecek: Cinder Logistics (kontrat avcısı), Harbor Crest (liman hakimi).

---

## 16. Zaman ve Gün Yapısı

| Mod | Gerçek süre | Kullanım |
|---|---|---|
| 1x | 1 oyun günü ≈ 24 gerçek dakika | Normal operasyon |
| 2x | ≈ 12 dk | Bekleme |
| 4x | ≈ 6 dk | Gece, uzun bekleme |
| Duraklat | — | Yönetim ekranları |

**Gün ritmi**

| Saat | Ne olur |
|---|---|
| 06:00 | Piyasa açılır, gecelik fiyatlar güncellenir |
| 07:00–09:00 | Trafik yoğun (+%35 seyahat süresi) |
| 08:00 | Haber bülteni |
| 09:00–17:00 | Tedarikçiler açık, denetimler bu saatlerde |
| 12:00 | Öğle haber güncellemesi |
| 17:00–19:00 | Trafik yoğun |
| 18:00 | Piyasa kapanış fiyatı, günlük özet |
| 22:00–05:00 | **Gri kanal açılır. Şüphe üretimi yarıya iner. Personel %50 zamlı** |

---

## 17. Arayüz Ekranları

| Ekran | Ne gösterir |
|---|---|
| **HUD** | Nakit, banka, şirket değeri, saat, aktif kontrat, **şüphe göstergesi**, elde taşınan kutu |
| **Piyasa** | Ürün listesi, güncel fiyat, trend oku, **fiyat geçmişi grafiği + kendi ortalama maliyet çizginiz** |
| **Tahmin Panosu** | Sinyaller, güven yüzdesi, maruziyet, taahhüt butonu |
| **Depo** | 3B doluluk ısı haritası, BU kullanımı, bozulma/eskime uyarıları, **stok farkı** |
| **Sipariş Terminali** | Tedarikçiler, kanal, fiyat, tedarik süresi, sepet |
| **Kontratlar** | Açık / kabul edilmiş / bitiş saati / ceza |
| **Finans** | Nakit akışı, borç, faiz, vergi, **kayıtlı vs. gerçek kâr** |
| **Uyum** | Lisanslar, denetim geçmişi, şüphe kırılımı, aktif riskler |
| **Şirket** | Personel, araç, yükseltme, itibar |
| **Not Defteri** | Geçmiş tahminler, sonuçları, "neden yanıldım" kartları |
| **Harita** | Seyahat noktaları, tedarikçi, alıcı, mesafe/süre/maliyet |
| **Gün Sonu Özeti** | Alınan/satılan, tahmin sonucu, nakit hareketi, şüphe değişimi |

**Görsel dil:** Koyu kömür gri zemin, sıcak kirik beyaz metin, sakin mavi-yeşil başarı vurgusu, kehribar uyarı, ölçülü kırmızı tehlike. Şüphe göstergesi tek renkli değil, dolgu seviyesiyle de okunur (renk körlüğü için).

---

## 18. Ses Yönü

| Katman | Yön |
|---|---|
| Müzik | Minimal elektronik/endüstriyel, 75–110 BPM, kontrat yaklaşınca gerilim yükselir |
| **Gri işlem** | Müzik incelir, ortam sesi öne çıkar, düşük frekanslı bir uğultu |
| **Denetim geliyor** | Müzik durur, sadece ayak sesi ve kapı |
| Piyasa tiki | Kısa mekanik darbe |
| Tahmin kilitleme | Ayırt edici iki notalı onay |
| Kâr | Kısa tatmin edici kasa sesi, kumarhane gibi değil |
| Zarar | Alçak, ölçülü darbe + kâğıt/terminal dokusu |
| Depo | Fan, forklift, palet, floresan uğultusu |
| Kutu | Karton sürtünmesi, bant sesi, istif tokatı |

---

## 19. Teknik Mimari

| Sistem | Yaklaşım |
|---|---|
| Ekonomi çekirdeği | **C++**, düşük frekanslı tick (dakikada 1) |
| Sunum ve arayüz | Blueprint + UMG |
| Veri | DataTable, **dış CSV pipeline** ile beslenir |
| Kayıt | Sürümlü SaveGame; tohum, gün, portföy, borç, lisans, şüphe |
| Rastgelelik | Alt sistem başına ayrı `FRandomStream` (market / olay / kontrat / lot / denetim) |
| Envanter | Veri odaklı; görsel için Instanced Static Mesh |
| NPC | Havuzlanmış, Animation Budget Allocator, Significance Manager |
| Aydınlatma | Baked ana ışık + az sayıda hareketli. **Lumen kapalı** |
| Nanite | Sadece binalarda; prop'larda kapalı |
| Harita | Tek harita, World Partition sadece iş akışını kolaylaştırıyorsa |

**Motor sürümü kuralı:** 2. ayda sabitlenir, proje boyunca yükseltilmez.

---

## 20. Early Access Çıkış İçeriği

Oyuncuyu tatmin edecek seviye hedeflenmiştir. Aşağıdakiler **çıkışta olacak**:

| Alan | Çıkış içeriği |
|---|---|
| Mod | Sandbox (sınırsız) + Hedef zinciri H1–H5 |
| Ürün | 34 SKU, dört katmanın hepsi temsil edilir |
| Marka | 24 marka, 8 üründe kalite kademesi |
| Araç | 5 (V01, V02, V04, V06, V07) + forklift |
| Kanal | Tezgâh, Terminal, Tablet, Liman, Acil, Gri — hepsi |
| **Gri pazar** | **Tam sistem: iki defter, lisans, şüphe, 4 denetim türü, gizleme araçları** |
| Olay | 20 |
| Kontrat | 10 şablon |
| Rakip | 2 |
| Personel | 8 rol, otomasyon merdiveni 0–7 basamak |
| Depo | Tüm yükseltmeler |
| Bölge | 5 (Depo, Liman, Eski Pazar, Perakende, Etkinlik) |
| Oynanış süresi | **12–18 saat doyurucu içerik** |
| Dil | TR, EN, zh-Hans, RU |

**Güncellemelerle gelecek:** +29 ürün, +16 olay, +10 kontrat, +2 rakip, H6–H9 hedefleri, 2 yeni bölge, meydan okuma modları, başarımlar.

---

## 21. Yapılmayacaklar Listesi

Bu liste bir sözleşmedir. Buradaki hiçbir şey kapsam içine alınmaz.

- Manuel araç kullanımı, trafik yapay zekâsı, araç hasarı, park etme
- Sürülebilir yol ağı ve şerit sistemi
- Gerçek fizikli kutu istifleme
- Yüzlerce benzersiz NPC ve günlük program simülasyonu
- Tüm şehir binalarının iç mekânı (3 tam iç mekân sınırı)
- Tam seslendirme (kısa tepki replikleri yeterli)
- Çok oyunculu
- Prosedürel bina inşa sistemi
- Bölüm başına ayrı harita
- Gerçek marka, logo veya ambalaj taklidi
- Yüzlerce benzersiz oynanış mantığı olan ürün
- Karakter oluşturma / üçüncü şahıs kamera

---

## 22. Riskler ve Karşı Önlemler

| Risk | İhtimal | Etki | Önlem |
|---|---|---|---|
| Kapsam kayması | Yüksek | Yüksek | Bu belgedeki sayı tavanları; "Yapılmayacaklar" listesi |
| Ekonomi ezberlenebilir olur | Orta | Yüksek | Tohumlu gizli olaylar; 100 tohumlu test |
| Ekonomi tamamen rastgele hisseder | Orta | Yüksek | Güven sinyalleri + "neden yanıldım" kartı |
| Gri sistem çok karmaşık gelir | Orta | Yüksek | Kademeli tanıtım (7.10); şüphe tek gösterge |
| Gri sistem çok kolay/kârlı olur | Orta | Yüksek | El koyma acısı görsel; şüphe birikimli |
| Arayüz Excel'e döner | Yüksek | Orta | Fiziksel dünya geri bildirimi; kısa karar ekranları |
| Depo performansı | Orta | Yüksek | Instanced mesh + mantıksal istif |
| NPC animasyon iş yükü | Yüksek | Orta | Modüler beden + paylaşılan animasyon |
| Marka benzerliği | Düşük/Orta | Yüksek | Kurgusal isimler + yayın öncesi marka taraması |
| Early Access içeriği yetersiz kalır | Orta | **Çok yüksek** | 12–18 saat hedefi; çıkış öncesi dış oyuncu testi |

---

## 23. Bitti Sayılma Kriterleri

- Oyuncu 12–18 saatte doyurucu bir sandbox deneyimi yaşayabilir.
- Gri pazar tamamen opsiyoneldir; beyaz oynayan oyuncu da oyunu tamamlayabilir.
- Her hedef en az bir yeni karar katmanı açar.
- Değer yoğunluğu ekseni oyuncuya öğretilmiştir (su vs. bileşen farkını bilir).
- Otomasyon merdiveninin her basamağı kutlanır ve fark edilir.
- El koyma anı görsel ve unutulmazdır.
- Yeni tohum görünür şekilde farklı bir piyasa hikâyesi üretir.
- Hiçbir gerçek marka, logo veya ambalaj kullanılmamıştır.
- Kayıt yükleme belirlenimcidir.
- Oyun ilk 20 dakikada anlaşılır, 5. ve 10. saatte derinleşir.

---

## Ek A — Kurumsal Kimlik Notu

Tüm şirket, ürün ve marka adları bu oyun için özgün ve kurgusaldır. Gerçek marka logoları, ticari kimlikler ve birebir tasarımlar kullanılmayacaktır. **Ticari yayından önce marka ve telif taraması yapılmalıdır.** Bu belge hukuki görüş içermez.
