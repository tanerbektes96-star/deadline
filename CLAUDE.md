# DEADLINE — Proje Talimatları (CLAUDE.md)

> **Yeniden kuruldu (2026-09-27).** Özgün kök `CLAUDE.md` hiç commit edilmemişti ve
> kayboldu. Bu sürüm, kaynak kodun ona yaptığı atıflardan ("CLAUDE.md mistake #5",
> "tick rule 6" gibi) ve tasarım belgelerinden geri çıkarıldı. Numaralar koddaki
> atıflarla birebir aynı tutuldu. Kodda izi olmayan numaralar **kayıp** olarak
> bırakıldı: o numaraları başka bir kurala verme, yoksa eski yorumlar yanlış kurala
> işaret eder. Kaybolan bir kural hatırlanırsa kendi numarasına yazılır.

Bu dosyanın tek geçerli kopyası budur (`Content/Deadline/CLAUDE.md` buraya yönlendirir).

---

## 1. Proje

- **Oyun:** DEADLINE — birinci şahıs lojistik / ticaret / tahmin oyunu. Tek geliştirici,
  kod Claude Code ile yazılıyor.
- **Motor:** Unreal Engine **5.8**, C++ projesi. Sürüm kilitli, **yükseltilmez**.
- **Modüller:** `DEADLINE_` (oyun, runtime) ve `DEADLINE_Editor` (sadece editör;
  `UDeadlineWidgetTools` Python'dan widget ağacı kurmak için var).
- **Belgeler** (`Content/Deadline/`):
  - `DEADLINE_GDD_v2.md`: ne yapılacağı. Kodda "GDD 8.1" gibi atıflar bölüm numarasıdır.
  - `DEADLINE_Roadmap_v2.md`: ne zaman. Biten madde `[x]` ile işaretlenir.
  - `DEADLINE_Sozluk.md`: terimler.
  - `DEADLINE_Karakter_Pipeline.md`: Tripo → Blender → Unreal karakter hattı.
- **Kapsam kuralı:** GDD'de olmayan sistem projeye girmez. Fikirler `FIKIRLER.md`'ye
  yazılır. GDD 21 "Yapılmayacaklar" listesi bir sözleşmedir.
- **Repo dışında kalanlar:** `EconomyPrototype/` (Python ekonomi referansı), `Assets/DEADLINE_UI_Prompts.md` (palet, font, UI
  referansı), `Assets/KULLANICI_GOREVLERI.md`, `HATA_GUNLUGU.md` (ör. H007). Kodda
  atıfları var ama depoda değiller. Gerekirse kullanıcıdan iste.

## 2. İş bölümü

- **Claude:** C++ taban sınıfları, alt sistemler, testler, editör Python betikleri
  (asset / WBP / DataTable kurulumu), belgeler.
- **Kullanıcı:** Unreal Editor'ü açmak, derlemek, betikleri çalıştırmak, oyunda test
  etmek, Tripo/Blender asset üretimi, hesap/kurulum işleri.
- Bulut oturumunda Unreal yoktur: kod derlenmeden commit edilir. Bunu kullanıcıya
  açıkça söyle, derleme ve test adımlarını yaz.

## 3. Mimari kuralları

- **Ekonomi çekirdeği C++'ta, sunum Blueprint/UMG'de** (GDD 19). Oyun mantığı
  widget'a veya HUD'a girmez; onlar sadece okur ve çizer.
- **Durum `UGameInstanceSubsystem`'lerde** yaşar (Time, Economy, Market, Inventory,
  Fleet, Travel, Event, News, Forecast, Save, ProductCatalog). Aktörler veriyi
  sahiplenmez, çizer.
- **Denge sayıları veride durur, C++'ta değil.** Ya DataTable/CSV'de (`DT_Products`,
  `DT_Vehicles`, `DT_Destinations`, `DT_Events`) ya da `UDeadlineSettings`'te
  (Project Settings > Game > Deadline, `Config/DefaultGame.ini`).
- **Rastgelelik:** alt sistem başına ayrı `FRandomStream`, hepsi kayıttaki
  `GameSeed`'den türetilir. Akış adları tohum türetmenin parçasıdır, yazımları
  değişmez: `MarketStream`, `EventStream` (sonrakiler: kontrat / lot / denetim).
  `EconomyPrototype/market.py` aynı dizeleri kullanır. C++ ile Python aynı tohumla aynı
  seriyi üretmeli (`MarketParityTest`).
- **Her sisteme en az bir elle tetikleme:** yeni alt sistem, `UDeadlineCheatManager`'a
  bir `Dl_` konsol komutu ekler.

## 4. Bilinen hatalar listesi (mistakes)

Koddaki "CLAUDE.md mistake #N" atıfları buraya işaret eder.

1. *(kayıp)*
2. **Envanter VERİDİR.** Dünyadaki kutular ayrı bir sunum katmanıdır. Depo defteri
   `UInventorySubsystem`'de, araç yükü `UFleetSubsystem`'de tutulur, aktörde değil.
3. *(kayıp)*
4. **Belirlenimcilik:** her şey tohumdan türetilir. Kaydet/yükle aynı piyasa hikâyesini
   yeniden üretmeli.
5. **Fizik simülasyonu yok, hiçbir zaman** (GDD 10.1). Kutular çarpışmaz, devrilmez.
   Rafa veya araca girince aktör olmaktan çıkar, Instanced Static Mesh olur.
6. **Beyaz mı gri mi?** Envanteri değiştiren her fonksiyon `bRecorded` alır. Çağıran
   her seferinde Fiziksel / Kayıtlı defter ayrımını (GDD 7.1) cevaplamak zorundadır.
7. **Ayar sayıları CSV'de**, yeniden derleme gerektirmeden değişebilsin.
8. **Kayıt yapısı sürüm numarasız çıkmaz.** Alan eklenince veya anlamı değişince
   `SaveVersion` artırılır, eski değer `USaveSubsystem::Migrate`'te ele alınır.

## 5. Tick kuralları

Varsayılan: **Tick yok.** Koddaki "tick rule N" / "method N" / "option N" atıfları:

1. **Tembel hesap (lazy evaluation):** değer, biri sorduğunda saatten hesaplanır. Saklanan
   şey geri sayım değil, mutlak zaman damgasıdır (ör. `AcquiredAtMinute`). Böylece
   kaydet/yükle veya seyahat atlaması "yetişme" gerektirmez.
2. **Abone ol, yoklama yapma:** UI ve aktörler delegelere bağlanır (`OnDayChanged`,
   `OnStockChanged`, `OnBoardChanged`…) ve sadece bir şey değişince yenilenir.
3. **Düşük frekanslı zamanlayıcı:** sürekli gereken iş Tick yerine timer ile yapılır (ör.
   odak ışını 10 Hz). Ekonomi tiki dakikada 1 (GDD 19).
4. *(kayıp)*
5. *(kayıp)*
6. **Koşullu tick:** tick yalnızca gerektiği anda açılır, iş bitince aynı karede kapanır.
   Örnek: yerine süzülen kutu (`AContainerActor`).

## 6. UMG deseni

- C++ taban sınıfı yaz, alt widget'ları **isimle bağla** (`meta = (BindWidget)`; olmasa
  da olur olanlar `BindWidgetOptional`). Eksik isim Blueprint derleme hatası verir. Bu
  bilerek böyle: sessizce boş ekran göstermek yerine yüksek sesle bozulur.
- WBP ağaçları `Content/Deadline/Core/build_*.py` betikleriyle kurulur
  (`unreal.DeadlineWidgetTools`). Betikler tekrar çalıştırılabilir. Ağacı sıfırdan
  kurarlar ve sınıf varsayılanlarını (ör. `CardWidgetClass`) ayarlarlar.
- Ekran sınıfları `UDeadlineSettings`'te soft class olarak durur, `DefaultGame.ini`'ye
  yazılır. Ekranı `ADeadlinePlayerController` açar ve girdi modunu o değiştirir.
- **Input Mapping:** `IMC_Default`'a tuş **eklenir, asla yeniden kurulmaz.**
  `unmap_all()` WASD'yi siler (HATA_GUNLUGU H007).
- **Palet** `UI/DeadlineUIPalette.h`'te, **sayı ve para biçimi** `UI/ForecastFormat.h`'te.
  Renk hiçbir zaman tek sinyal değildir, yanında sayı veya metin de basılır.
- **Unity build:** .cpp'lerde anonim namespace içinde ortak isimli yardımcı tanımlama.
  Aynı unity dosyasına düşünce çakışır. Paylaşılan yardımcılar başlık dosyasında
  `inline` olarak durur.

## 7. Dil

- Oyunun varsayılan dili **Türkçe** (`UDeadlineSettings::GameLanguage = "tr"`, çalışırken
  `-DeadlineLang=en`).
- Koddaki sabit metinler `NSLOCTEXT("Deadline", "<BenzersizAnahtar>", "Türkçe metin")`
  biçimindedir. **Aynı anahtar iki farklı metinle kullanılmaz.**
- Veri tablolarında TR/EN sütunları vardır; `DeadlineLocale::Pick`, `DL_PRINTF` seçer.
- Kod yorumları ve commit mesajları İngilizce, belgeler ve kullanıcıyla konuşma Türkçe.

## 8. Testler ve komutlar

Otomasyon testleri `Source/DEADLINE_/Tests/`, adları `Deadline.<Sistem>.<Konu>`:

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
  "C:\Users\PC\Desktop\DEADLINE_\DEADLINE_.uproject" ^
  -ExecCmds="Automation RunTests Deadline;Quit" -unattended -nopause -nosplash -nullrhi -log
```

Editör betiği çalıştırma:

```
UnrealEditor-Cmd.exe DEADLINE_.uproject -ExecutePythonScript=<betik> -unattended -nosplash -nop4 -RenderOffscreen
```

Saf mantık (ör. `PickLesson`, `Summarise`) statik fonksiyon olarak yazılır ki elle kurulmuş
verilerle test edilebilsin.

## 9. Git

- `main` dalı, GitHub: `tanerbektes96-star/deadline` (private). Kullanıcının makinesindeki
  klasör: `C:\Users\PC\Desktop\DEADLINE_`.
- `.uasset`, `.umap`, görsel, FBX ve Blender dosyaları **Git LFS**'te (`.gitattributes`).
- `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/` commit edilmez.
- Bulutta commit atıldıysa kullanıcı bilgisayarında önce `git pull` yapar.
