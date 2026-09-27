# DEADLINE — 12 Aylık Geliştirme Yol Haritası v2.0

**Hedef:** 10. ayda doyurucu bir Early Access çıkışı, ardından içerik güncellemeleriyle 1.0.
**Ekip:** 1 geliştirici. Kodlama Claude Code ile yapılacak.
**Motor:** Unreal Engine 5 (sürüm 2. ayda kilitlenir).

> Terim sözlüğü: `DEADLINE_Sozluk.md`

---

## 1. Yol Haritası Kullanım Kuralları

1. Her ay **kur → test et → kilitle** sırasıyla ilerler. Ay sonu kontrol listesi geçilmeden bir sonraki ayın bağımlı işine geçilmez.
2. Her sistem önce **placeholder** (geçici, gri kutu) asset ile kurulur. Final asset en sona bırakılır.
3. Yeni özellik ekleme, ancak P0 (oyunu bozan) hataların hepsi kapandıktan sonra yapılır.
4. Her ay sonunda çalışan bir build alınır ve saklanır.
5. Bu belgede olmayan hiçbir sistem projeye girmez. Fikir gelirse `FIKIRLER.md` dosyasına yazılır, sonraki sürümde değerlendirilir.

---

## 2. AY 0 — Kod Yazmaya Başlamadan Önce

Bu ayın çıktısı Unreal'de tek satır kod değil. **Bu ay bitmeden Unreal projesini açmayın.**

### 2.1 Ekonomi prototipi (en kritik iş) — Python, Excel değil

İlk planda Excel önermiştim. **Python betiği daha iyi** ve Claude Code bunu sizin yerinize yazıp çalıştırabilir:

| Excel | Python betiği |
|---|---|
| Elle formül yazılır | Claude Code yazar |
| 20 tohum koşumu elle yapılır | Tek komutla 100 tohum koşar |
| Sonuçlar elle incelenir | Otomatik test raporu ve grafik üretir |
| Kod olarak kullanılamaz | **C++ ekonomi çekirdeğinin referans uygulaması olur** |
| Sürüm takibi zor | Git'te takip edilir |

Son madde önemli: Python prototipi sonradan C++'a çevrilirken **karşılaştırma referansı** olur. Aynı tohumla Python ve C++ aynı fiyat serisini üretmeli. Bu, ekonomi hatalarını yakalamanın en güvenilir yolu.

**Üretilecek dosyalar:**

```
EconomyPrototype/
  products.py        (CSV okuma)
  market.py          (ortalamaya dönüşlü fiyat modeli — GDD 8.1)
  events.py          (20 olay, tohumlu takvim)
  agent.py           (basit "ucuza al pahalıya sat" test botu)
  run_tests.py       (100 tohum × 200 gün, rapor + grafik)
  README.md
```

**Geçmesi gereken testler:**

- Hiçbir ürün 20 koşumun 15'inden fazlasında en kârlı olmamalı.
- Hiçbir fiyat kendi risk bandının sınırlarını aşmamalı.
- Kriz bandındaki bir ürün en az bir kez 3 katına çıkmalı.
- Stable bandındaki bir ürün hiçbir zaman 1.2 katını geçmemeli.
- 200 günün sonunda oyuncu simülasyonu (basit "ucuza al pahalıya sat" botu) kâr etmeli ama zengin olmamalı.

Bu testleri geçmeden ekonomi kodu yazılmaz.

### 2.2 Kod yazmaya başlamak için gereken asset listesi

**Kısa cevap: neredeyse hiçbir şey.** Unreal'in içindeki hazır küpler ve silindirlerle başlanır. Aşağıdaki tablo kesin gereksinimdir.

| Öncelik | Asset | Nasıl elde edilir | Ne zaman gerekir |
|---|---|---|---|
| **P0** | Kutu S / M / L | **Ölçekli küp.** Unreal'in `Cube` mesh'i, 0.3 / 0.4 / 0.6 m | Gün 1 |
| **P0** | Palet | Ölçekli küp, 1.2 × 0.8 × 1.4 m | Gün 1 |
| **P0** | Raf | 3 ölçekli küp (2 dikey + 3 yatay) | Gün 1 |
| **P0** | Zemin / duvar | Unreal `Plane` ve `Cube` | Gün 1 |
| **P0** | Araç gövdesi | Ölçekli küp + tekerlek silindirleri | Gün 1 |
| **P1** | Forklift | Ölçekli küp + çatal | 3. ay |
| **P1** | NPC | Unreal'in hazır `Manny`/`Quinn` karakteri | 5. ay |
| **P2** | Gerçek modeller | Görsel üretim + modelleme | **9. ay** |

**Önemli:** Gerçek 3B modeller **9. aya kadar gerekmiyor.** Oynanışın tamamı gri kutularla kurulabilir ve kurulmalıdır. Sanat üretimine erken başlamak, tek kişilik projelerin en yaygın ölüm sebebidir.

### 2.3 Unreal projesini kurma adımları

1. **Yeni proje:** Games → Blank → **C++** (Blueprint değil). Starter Content **kapalı**.
2. **Proje adı:** `Deadline`
3. **Motor sürümü:** UE 5.x — hangi sürümü seçerseniz seçin, not alın ve **bir daha yükseltmeyin**.
4. **Klasör yapısı:**

```
Content/
  Deadline/
    Core/            (temel oyun sınıfları)
    Data/            (DataTable ve Data Asset)
    Gameplay/
      Inventory/
      Economy/
      Contracts/
      Compliance/    (gri pazar ve denetim)
      Logistics/
      Staff/
    Actors/
      Containers/    (kutu, palet, kasa)
      Vehicles/
      Interactables/
    Characters/
      Player/
      NPC/
    Environment/
      Modular/
      Buildings/
      Props/
    UI/
      Widgets/
      Icons/
    Audio/
    Materials/
      Masters/
      Instances/
    Textures/
      Labels/
      Surfaces/
```

5. **Kaynak kontrolü:** Git kurun. `.gitignore` içine `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/` ekleyin. **Git LFS** kurup `*.uasset`, `*.umap`, `*.png`, `*.fbx` için etkinleştirin.
6. **CLAUDE.md** dosyasını proje köküne kopyalayın.
7. **`Data/DT_Products.csv`** dosyasını `Content/Deadline/Data/` altına kopyalayın.

### 2.4 Ay 0 görev dağılımı

**Siz yapacaksınız (Claude Code yapamaz):**

- [ ] Epic Games Launcher ve Unreal Engine kurulumu
- [ ] Git ve Git LFS program kurulumu (yönetici izni gerekir)
- [ ] Python kurulumu (yoksa)
- [ ] İlk UE projesini Editor'den oluşturmak: **Games → Blank → C++**, Starter Content kapalı, proje adı `Deadline`
- [ ] Motor sürümünü not etmek

**Claude Code yapacak:**

- [ ] `.gitignore`, `.gitattributes`, LFS yapılandırması, ilk commit
- [ ] Klasör yapısını oluşturmak
- [ ] `CLAUDE.md` ve `Data/DT_Products.csv` dosyalarını yerine koymak
- [ ] Python ekonomi prototipini yazmak
- [ ] 100 tohumlu testi çalıştırmak ve **sonuç raporunu size sunmak**
- [ ] Testler geçmezse modeli düzeltip tekrar koşmak
- [ ] CSV'yi DataTable olarak okuyan ilk C++ struct'ını yazmak

### 2.5 Ay 0 kontrol listesi

- [ ] Ekonomi prototipi 5 testin hepsini geçiyor (rapor elinizde)
- [ ] Unreal C++ projesi açıldı, klasör yapısı kuruldu
- [ ] Git + LFS çalışıyor, ilk commit atıldı
- [ ] `CLAUDE.md` proje kökünde
- [ ] `DT_Products.csv` içeri aktarıldı ve DataTable olarak okunuyor
- [ ] Motor sürümü not edildi

---

## 3. Aylık Plan

### AY 1 — Temel ve Greybox

**Çıktı:** Oyuncu gri bir depoda yürüyor, gri kutuları alıp bırakabiliyor, para bakiyesi değişiyor.

- [ ] `ADeadlinePlayerCharacter` — birinci şahıs hareket, FOV ayarı
- [ ] Etkileşim sistemi (ışın izleme + arayüz)
- [ ] `AContainerActor` — Kutu S/M/L, alma/bırakma, taşıma hız cezası
- [ ] `UInventorySubsystem` — BU tabanlı kapasite hesabı
- [ ] `UEconomySubsystem` — nakit, banka, temel işlem kaydı
- [ ] `DT_Products` okuma ve tek ürünle test alım-satımı
- [ ] Greybox depo + rampa + bir tedarikçi tezgâhı
- [ ] Kayıt/yükleme iskeleti (sürüm numaralı)
- [ ] Geliştirici hileleri: para ekle, zamanı ilerlet, kutu oluştur

**Kapı testi:** 10 dakikalık oynanabilir dilim. Kutu al, tezgâha götür, sat, para artsın. Kaydet, çık, yükle, aynı durumda devam et.

---

### AY 2 — Piyasa ve Fiyat Simülasyonu

**Çıktı:** 12 ürün ekonomik olarak farklı davranıyor, fiyatlar mantıklı hareket ediyor.

- [ ] `UMarketSubsystem` — ortalamaya dönüşlü fiyat modeli (GDD 8.1)
- [ ] Risk bantları ve sert sınırlar
- [ ] Günlük fiyat tiki (C++, dakikada 1)
- [ ] **CSV → DataTable yeniden yükleme aracı** (oyun kapanmadan denge ayarı)
- [ ] Alım/satım ekranı (basit UMG)
- [ ] Fiyat geçmişi grafiği + ortalama maliyet çizgisi
- [ ] İşlem geçmişi ve kâr/zarar hesabı
- [ ] Alt sistem başına ayrı `FRandomStream`
- [ ] **Motor sürümü kilitlenir**
- [ ] **Steamworks kâğıt işleri başlatılır** — hesap, 100 USD Steam Direct ücreti, vergi anketi, banka doğrulama. Bunlar haftalar sürebilir, bu yüzden şimdi başlatılır (bkz. `DEADLINE_Steam_Cikis_Plani.md` Bölüm 3.1)

**Kapı testi:** 12 ürün 20 gün boyunca farklı davranıyor. Hiçbir fiyat patlamıyor. Excel prototipiyle sonuçlar uyumlu.

---

### AY 3 — Depo ve Lojistik

**Çıktı:** Fiziksel operasyon ekonomik kararı destekliyor.

- [ ] Raf, palet alanı, soğuk bölge, kafesli raf sistemleri
- [ ] Palet aç/kapat (16 kutuya ayır / tek modele topla)
- [x] **Transpalet** (forklift yerine — karar aşağıda)
- [ ] `AVehicleActor` — kargo yuvaları + Instanced Static Mesh istifleme
- [ ] **Yükleme akışı:** kutu → tetikleme alanı → F → yuvaya süzülme
- [ ] **Seyahat Et sistemi:** araç → E → harita → hedef → 2-3 sn geçiş
- [ ] Rota maliyeti (mesafe, trafik, yakıt, ağırlık)
- [ ] Bozulma ve eskime sayaçları + görsel durum

**Kapı testi:** Kutuyu depodan al, kamyona yükle, seyahat et, alıcıya teslim et. Kamyon dolarken görünüyor. Kapasite BU olarak doğru hesaplanıyor.

> **Karar (2026-09-05): sürülebilir forklift yok, transpalet var.**
>
> Bu satır önce "Forklift (basit, fizik motorsuz, kinematik)" diyordu ve GDD 21
> ile çelişiyordu ("Manuel araç kullanımı" yapılmayacaklar listesinde). GDD 5.2
> zaten paletler için **"forklift/transpalet"** diyor, yani transpalet en baştan
> izinliydi.
>
> Kararı veren şey sürüş zevki değil, **otomasyon merdiveni**. GDD 10.3 basamak
> 4'te bu işi bir NPC devralacak. Yürüyen bir işçi `AIController` + NavMesh
> demek — motor bunu hazır veriyor. Süren bir işçi ise depo içi araç
> navigasyonu demek (dönüş yarıçapı, geri manevra, çatal hizalama) ve iş **iki
> kez** yazılır: bir kez oyuncu için, bir kez AI için. GDD 9.1 bu problem
> sınıfını şehir için zaten bir kez iptal etmişti.
>
> Sürülebilir forklift ileride bir **oyuncak** olarak eklenebilir; aktarma
> mantığı aynı, sadece hareket değişir.

---

### AY 4 — Tahmin, Olaylar ve Steam Sayfası

**Çıktı:** Oyunun imza anı tek başına eğlenceli. Mağaza sayfası yayında.

Bu ay ikiye bölünür: ilk iki hafta **vitrin**, son iki hafta **oynanış**.

**Hafta 1–2 — Vitrin işi (mağaza sayfası için)**

- [ ] **Deponun tek bir köşesi final kaliteye getirilir** — raflar, kutular, ışık, zemin, ses. Haritanın geri kalanı gri kalır
- [ ] 10–12 ürün için gerçek model + etiket
- [ ] Bir araç final kalitede
- [ ] 8 ekran görüntüsü çekilir (1920×1080, PNG)
- [ ] 60–90 saniyelik fragman çekilir ve kurgulanır
- [ ] 7 Steam kapsül görseli + logo üretilir (`DEADLINE_UI_Prompts.md` Bölüm 4)
- [ ] Kısa açıklama, uzun açıklama, Erken Erişim anketi yazılır
- [ ] **Mağaza sayfası yayına girer** → wishlist saati başlar

**Hafta 3–4 — Tahmin sistemi**

- [x] `UEventSubsystem` — 12 olay, tohumlu takvim
- [x] Haber/radyo katmanı (aynı olay verisinden beslenir)
- [x] Tahmin panosu: sinyaller, güven yüzdesi, maruziyet
- [x] Taahhüt akışı: miktar + bütçe kilidi + süre
- [x] Sonuç ekranı: doğru/yanlış + **"neden yanıldım" kartı**
- [x] Not defteri (geçmiş tahminler)
- [ ] **Gün sonu özeti ekranı**
- [ ] Büyük geri bildirim durumları: KAZANÇ / KAYIP / KRİZ

**Kapı testi:** Tahmin kararı 30 saniyede anlaşılıyor. En az 6 olay tekrar oynanışta farklı sonuç üretiyor. Mağaza sayfasındaki 8 ekran görüntüsünün hiçbiri utandırıcı değil.

> **Neden Ay 3 değil de Ay 4?** İlk raporda "Ay 3'te greybox görsellerle sayfayı açın" demiştim; bu tavsiye yanlıştı. Greybox ekran görüntüsüyle açılan sayfa wishlist toplamaz, sadece ilk izlenim hakkınızı harcar. Bir haftalık vitrin işi bu farkı kapatır. Gerekçesi `DEADLINE_Steam_Cikis_Plani.md` Bölüm 2'de.

---

### AY 5 — Kontratlar, Personel ve Otomasyon

**Çıktı:** Oyuncu tek kişi değil, küçük bir şirket yönetmeye başlıyor.

- [ ] 10 kontrat şablonu, kabul/tamamlama/başarısızlık
- [ ] SLA süresi, ceza hesabı, itibar
- [ ] 8 personel rolü, işe alım, maaş
- [ ] **Otomasyon merdiveni basamak 1–5** (mal kabul → raflama → paketleme → yükleme → seyahat)
- [ ] Her basamak açılışında kutlama + kazanılan süre bildirimi
- [ ] Personel yapay zekâsı: basit durum makinesi, navmesh üzerinde taşıma
- [ ] Sarf malzemesi sistemi (kutu, bant, palet, yakıt, etiket)
- [ ] Hedef zinciri H1–H2

**Kapı testi:** 2 çalışan görev akışı çalışıyor. Kontrat başarı/başarısızlığı doğru hesaplanıyor. Otomasyon açılınca oyuncu farkı hissediyor.

---

### AY 6 — Yaşayan Şehir

**Çıktı:** Depo ve şehir merkezi artık boş harita hissi vermiyor.

**Sırayla yapın. İlk dördü bittiğinde durup değerlendirin — yaya sayısını artırmanız gerekmeyebilir.**

- [ ] **Katman 1:** Bölgesel ses yatakları (trafik, liman, forklift, floresan, anons)
- [ ] **Katman 2:** Animasyonlu dekor (aspiratör, branda, rampa lambası, buhar, konveyör, uzak vinç)
- [ ] **Katman 3:** Uzak trafik (spline üzerinde, AI'sız, çarpışmasız, 12–20 araç)
- [ ] **Katman 4:** Silüet NPC (pencere arkası, üst geçit, uzak iskele)
- [ ] **Katman 5:** Gerçek yayalar — 8–14 havuzlanmış, 3 durumlu
- [ ] **Katman 6:** Park hâlindeki araçlar (statik)
- [ ] **Katman 7:** Mikro aktivite sahneleri (yol çalışması, çöp, kurye, kepenk)
- [ ] **Katman 8:** Etkinlik kalabalığı preset'i (sadece etkinlik bölgesi, 20–28)
- [ ] Animation Budget Allocator + Significance Manager
- [ ] Gündüz/gece yoğunluk preset'leri

**Kapı testi:** Şehir merkezinde 30 saniyelik yürüyüş boş hissettirmiyor. FPS hedefi içinde. Ambient NPC oyuncuya bakmıyor ve etkileşim ipucu göstermiyor.

---

### AY 7 — Gri Pazar, Denetim ve Demo

**Çıktı:** Oyunun farklılaştırıcısı çalışıyor. Demo build hazır.

- [ ] **İki defter sistemi:** Fiziksel Stok vs. Kayıtlı Stok, Stok Farkı hesabı
- [ ] **Nakit / Banka ayrımı**, çekim eşiği
- [ ] **Lisans sistemi:** 6 izin, askıya alma, iptal
- [ ] **Şüphe (Heat) sayacı:** artıranlar, düşürenler, bantlar
- [ ] **4 denetim türü:** rutin, nokta, liman, vergi
- [ ] 4 devlet NPC'si (G01–G04)
- [ ] **El koyma sekansı** — denetçiler gelir, sayar, forkliftle götürür (görsel)
- [ ] Gizleme araçları: Güvenli Oda, sahte irsaliye, liman konteyneri, gece sevkiyatı, karışık yükleme
- [ ] **Gri kanal:** gece, fiziksel, nakitle, belirli noktalarda
- [ ] **Seyahat olayları** (yol kontrolü dahil)
- [ ] Fiyat fahişliği takibi ve muhabir tepkisi
- [ ] Uyum ekranı
- [ ] Hedef zinciri H3–H5
- [ ] **Demo build:** 45–60 dakikalık tek oturum

**Kapı testi:** Gri yol da beyaz yol da oynanabiliyor. Yeni oyuncu gri sistemi 3 adımda öğreniyor. El koyma anı acı veriyor.

---

### AY 8 — Rakipler, Next Fest, Denge

**Çıktı:** Wishlist sıçraması. Ekonomi tek stratejiye düşmüyor.

- [ ] 2 rakip (soyut karar katmanı, fiziksel simülasyon yok)
- [ ] Rakip görünürlüğü: haber, fiyat, tükenmiş tezgâh, ihale, ihbar
- [ ] Bilgi ürünleri: piyasa raporu, liman durumu, talep nabzı, hava sinyali
- [ ] Yanlış sinyal kuralları
- [ ] **Steam Next Fest katılımı** (demo hazır olmadan girmeyin)
- [ ] Denge geçişi: 100 tohumlu ekonomi testi
- [ ] Meydan okuma değiştiricileri (opsiyonel)

**Kapı testi:** Rakip en az 3 stratejiye tepki veriyor. Oyuncu neden kaybettiğini anlıyor. Hiçbir ürün her koşumda kazanan değil.

---

### AY 9 — İçerik Tamamlama

**Çıktı:** Çıkış kapsamındaki tüm asset'ler yerinde. **Gerçek 3B modeller bu ay geliyor.**

- [ ] 34 ürün modeli (7 kap + 34 etiket)
- [ ] 24 marka etiket seti
- [ ] 5 araç + forklift
- [ ] 8 benzersiz NPC + 10 varyant + ambient sistem
- [ ] 40 modüler çevre parçası
- [ ] 3 tam iç mekân + 5 cephe binası
- [ ] Ses ve görsel efekt geçişi
- [ ] Tüm kurgusal tabela ve markalama
- [ ] Lokalizasyon: TR, EN, zh-Hans, RU
- [ ] Erişilebilirlik: metin boyutu, renkten bağımsız durum işaretleri

**Kapı testi:** GDD asset listesi %100. Görsel stil tüm bölgelerde tutarlı. Hiçbir yerde placeholder kalmadı.

---

### AY 10 — Early Access Çıkışı

**Çıktı:** Doyurucu, kararlı bir çıkış sürümü.

- [ ] 12–18 saatlik tam oturum testi
- [ ] Yeni Oyun / Yükle / Devam / İflas / Kurtarma testleri
- [ ] Tüm kontrat ve olay regresyonu
- [ ] Performans doğrulaması (1080p – 4K profilleri)
- [ ] Kilitlenme analizi kancaları, kayıt yedekleme
- [ ] **Dış oyuncu testi: en az 8 kişi, en az 4 saat oynasın**
- [ ] Steam build, başarımlar, ekran görüntüleri, fragman
- [ ] Yayıncı build'i (çıkıştan 2 hafta önce)
- [ ] RC1 → düzeltmeler → RC2 → **çıkış**

**Çıkış eşiği:** En az 8.000 wishlist. Altındaysa çıkışı 4–6 hafta erteleyin ve pazarlamaya devam edin.

---

### AY 11–12 — Çıkış Sonrası

- [ ] İlk hafta: sadece hata düzeltme. Yeni özellik yok
- [ ] 2. hafta: ilk denge yaması, oyuncu geri bildirimine göre
- [ ] 6. hafta: **Güncelleme 1** — +17 ürün, +8 olay, H6 hedefi, 1 yeni rakip
- [ ] 12. hafta: **Güncelleme 2** — +12 ürün, +8 olay, H7–H8, 1 rakip, 1 yeni bölge
- [ ] Yol haritası sayfası Steam'de yayında tutulur

---

## 4. Güncelleme Takvimi (Çıkış Sonrası)

| Güncelleme | Süre | İçerik |
|---|---|---|
| **U1** | Çıkış + 6 hafta | 17 ürün (kartuş, telefon, SSD, lityum, alet, kakao, protein vb.), 8 olay, 5 kontrat, H6 hedefi, Cinder Logistics rakibi |
| **U2** | Çıkış + 12 hafta | 12 ürün (altın, lüks, kimyasal, tekstil, inşaat, ÖTV'li mal), 8 olay, 5 kontrat, H7–H8, Harbor Crest rakibi, Sanayi Bölgesi |
| **U3** | Çıkış + 20 hafta | H9 final hedefi, meydan okuma modları, günlük senaryo, başarım seti |
| **1.0** | Çıkış + 28–36 hafta | Tam hedef zinciri, denge kilidi, fiyat 19.99 $ |

---

## 5. Haftalık Çalışma Kuralı

Part-time geliştirme için aylık hedefler haftalara bölünür.

| Hafta | Odak | Çıktı |
|---|---|---|
| W1 | Plan + veri + greybox | Görev listesi, çalışan dal |
| W2 | Uygulama | Sistem izole olarak oynanabilir |
| W3 | İçerik + entegrasyon | Sistem ana döngüye bağlandı |
| W4 | Test + cila | Aylık kontrol build'i |

**Genel kural:** Haftanın ilk üçte ikisi üretim, son üçte biri test ve hata düzeltme.

---

## 6. Bitti Sayılma Tanımı (her özellik için)

Bir özellik ancak şu altısı sağlanınca "bitti" sayılır:

1. Temiz bir build'de oynanabiliyor.
2. Gerekiyorsa kayıt/yükleme sistemine bağlı.
3. Bilinen engelleyici veya kritik hata yok.
4. Final ya da onaylanmış geçici asset kullanıyor.
5. Planlanmamış sistemsel kapsam eklemiyor.
6. GDD ana kontrol listesine karşı doğrulandı.

---

## 7. Pazarlama Takvimi

| Ne zaman | Ne | Hedef |
|---|---|---|
| Ay 2 | Steamworks kâğıt işleri başlar | Banka doğrulaması haftalar sürer |
| Ay 4 | **Steam sayfası açılır** (vitrin haftası sonrası) | Wishlist saati işlemeye başlasın |
| Ay 4–9 | Haftalık kısa video/GIF | Her klip tek bir mekanik gösterir |
| Ay 7 | Demo yayında | Kalıcı olarak açık kalsın |
| Ay 8 | Steam Next Fest | En büyük tek wishlist sıçraması |
| Ay 9 sonu | Yayıncı build'i | Orta ölçekli tür kanalları |
| Ay 10 | Early Access çıkışı | 12.99–14.99 $, min. 8.000 wishlist |
| Ay 10+ | 6 haftada bir güncelleme | Steam algoritmasında görünürlük yenilenir |

**En iyi klip malzemeleri (fragmanda bunlar olsun):**

1. Manifestosuz lotun depoda açılması
2. Denetçilerin gelip mala el koyması
3. Bir tahminin tuttuğu an, fiyat grafiği yukarı fırlarken
4. Tıka basa dolu bir kamyonun kapısının kapanması
5. Yol kontrol noktasında "riske gir" kararı
6. İlk şoförün işe alınması ("artık işi ben yapmıyorum")

---

## 8. Ana Kontrol Listesi — Early Access Kapsamı

- [ ] Ana döngü: bilgi → tahmin → alım → depolama → satış → sonuç → yatırım
- [ ] Sandbox modu, sınırsız
- [ ] Hedef zinciri H1–H5
- [ ] 34 ürün, 24 marka, 8 üründe kalite kademesi
- [ ] 7 kap tipi, BU sistemi
- [ ] 5 araç + forklift
- [ ] 6 sipariş kanalı
- [ ] Gri pazar tam sistemi (iki defter, lisans, şüphe, 4 denetim, gizleme)
- [ ] 4 devlet denetim NPC'si
- [ ] Seyahat sistemi + 7 seyahat olayı
- [ ] Otomasyon merdiveni 0–7
- [ ] 8 personel rolü
- [ ] 20 piyasa olayı
- [ ] 10 kontrat şablonu
- [ ] 2 rakip
- [ ] Depo yükseltmeleri
- [ ] 8–14 ambient yaya + 8 canlılık katmanı
- [ ] 40 modüler parça, 3 iç mekân, 5 cephe
- [ ] Fiyat geçmişi grafiği, gün sonu özeti, depo ısı haritası
- [ ] Tohumlu belirlenimci kayıt
- [ ] 4 dil
- [ ] Steam build, başarımlar, fragman
