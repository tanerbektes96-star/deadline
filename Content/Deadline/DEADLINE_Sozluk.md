# DEADLINE — Terimler Sözlüğü

Bu dosya, diğer belgelerde geçen ve aşina olmayabileceğiniz her terimi basit dille açıklar.
Bir belgede tanımadığınız bir kelime görürseniz buraya bakın. Terimler konu başlıklarına göre gruplandı.

---

## 1. Oyun Geliştirme / Unreal Engine Terimleri

| Terim | Basit açıklama |
|---|---|
| **Asset (varlık)** | Oyunun içine koyduğunuz her dosya: 3B model, doku, ses, animasyon. "Asset üret" = "bu dosyaları hazırla" demek. |
| **Mesh (model)** | Bir nesnenin 3 boyutlu şekli. Kutunun kendisi bir mesh'tir. |
| **Static Mesh** | Hareket etmeyen, kemiği olmayan model. Kutu, raf, bina. |
| **Skeletal Mesh** | İçinde iskelet olan, animasyon oynatabilen model. İnsanlar, forklift kolu. |
| **Material (materyal)** | Bir modelin yüzeyinin nasıl göründüğünü tarif eden ayar. "Bu karton, bu metal, bu cam" bilgisi. |
| **Master Material** | Ana materyal şablonu. Bir kere yaparsınız, sonra kopyalarını farklı ayarlarla kullanırsınız. |
| **Material Instance (MI)** | Master Material'in ayarları değiştirilmiş kopyası. Yeni materyal yazmadan sadece rengi/dokusu değişir. Çok ucuzdur. |
| **Texture (doku)** | Modelin üzerine giydirilen resim. Karton deseni, etiket, logo. |
| **UV / UV Unwrap** | 3B modeli kesip düzleştirip 2B kâğıda serme işlemi. Dokunun modelin neresine oturacağını bu belirler. Kutuyu açıp düz kartona sermek gibi düşünün. |
| **UV Island (UV adası)** | Düzleştirilmiş modelin bir parçası. Kutunun ön yüzü ayrı bir "ada" olabilir. |
| **Material Slot (materyal yuvası)** | Bir modelin farklı yüzeylerine farklı materyal atayabilme özelliği. Kutunun gövdesi karton, ön yüzü etiket olabilir. |
| **Decal (çıkartma)** | Yüzeye sonradan yapıştırılan görsel. Duvardaki leke, yerdeki çizgi, kutudaki etiket. |
| **Atlas / Texture Atlas** | Birçok küçük resmi tek büyük resim dosyasında toplamak. 50 etiketi tek dosyaya sığdırmak gibi. Performans için yapılır. |
| **Trim Sheet** | Tek bir dokuda birçok kenar/detay şeridi bulundurmak. Modüler binalarda çok kullanılır. |
| **PBR** | "Physically Based Rendering". Yüzeylerin ışığa gerçekçi tepki vermesini sağlayan modern standart. Pürüzlülük ve metaliklik gibi ayarlar buna dahildir. |
| **Albedo / Base Color** | Bir yüzeyin ışıktan bağımsız saf rengi. Gölgesiz, parlamasız hâli. |
| **Roughness (pürüzlülük)** | Yüzeyin ne kadar mat ya da parlak olduğu. 0 = ayna, 1 = tebeşir. |
| **Normal Map** | Modeli daha detaylı göstermek için kullanılan sahte kabartma dokusu. Poligon eklemeden girinti çıkıntı hissi verir. |
| **LOD (Level of Detail)** | Uzaktaki nesnelerin daha basit modele geçmesi. Performans için şart. |
| **Instanced Static Mesh (ISM)** | Aynı modelden yüzlerce kopyayı tek çizim işlemiyle ekrana basma yöntemi. 200 kutu için 200 ayrı yük değil, 1 yük oluşur. |
| **Draw Call (çizim çağrısı)** | Ekran kartına verilen her "şunu çiz" komutu. Sayısı arttıkça oyun yavaşlar. Azaltmak iyidir. |
| **Nanite** | UE5'in çok yüksek poligonlu modelleri verimli gösterme sistemi. Düşük poligonlu nesnelerde faydası yok, hatta zarar. |
| **Lumen** | UE5'in gerçek zamanlı ışık sistemi. Güzel ama pahalıdır; bilgisayarı yorar. |
| **Baked Lighting (pişmiş ışık)** | Işığın önceden hesaplanıp dokuya kaydedilmesi. Çok hızlı çalışır ama ışık hareket edemez. |
| **Blueprint (BP)** | Unreal'in görsel kodlama sistemi. Kod yazmadan kutu-ok bağlayarak mantık kurarsınız. |
| **C++** | Unreal'in asıl programlama dili. Blueprint'ten çok daha hızlı çalışır. Ekonomi hesapları burada olmalı. |
| **DataTable (DT)** | Unreal içindeki tablo. Excel/CSV dosyasından okunur. Ürün listesi gibi verileri burada tutarsınız. |
| **CSV** | Virgülle ayrılmış basit tablo dosyası. Excel'de açılır, Unreal'e aktarılır. Denge ayarı için ideal. |
| **Data Asset** | Tek bir ayar paketini tutan Unreal dosyası. DataTable'ın tek satırlık hâli gibi. |
| **Actor** | Oyun dünyasına yerleştirilen her şey. Kutu, NPC, kamera, ışık. |
| **Pawn / Character** | Kontrol edilebilen actor. Oyuncu karakteri bir Character'dır. |
| **Subsystem** | Oyun boyunca tek kopya yaşayan yönetici sınıf. "Ekonomi Yöneticisi" gibi. |
| **Tick** | Her karede çalışan güncelleme fonksiyonu. Çok kullanmak oyunu yavaşlatır. |
| **Pooling (havuzlama)** | Nesneleri sürekli yaratıp yok etmek yerine, hazır bir havuzdan alıp geri koymak. NPC'ler için şart. |
| **Spawn / Despawn** | Nesneyi dünyada oluşturmak / kaldırmak. |
| **Navmesh** | Yapay zekânın üzerinde yürüyebileceğini bildiği zemin haritası. |
| **State Machine (durum makinesi)** | "Yürüyor / bekliyor / taşıyor" gibi durumlar arasında geçiş yapan basit mantık. |
| **Anim BP** | Animation Blueprint. Bir karakterin hangi animasyonu ne zaman oynatacağını belirleyen dosya. |
| **Leader Pose Component** | Bir karakterin kıyafet parçalarının ana bedenle aynı hareketi yapmasını sağlayan Unreal özelliği. Modüler karakter için temel araç. |
| **Skeletal Mesh Merge** | Ayrı kıyafet parçalarını çalışma anında tek bir modelde birleştirme. Performans kazandırır. |
| **Morph Target** | Modelin şeklini kaydırma ayarı. Zayıf/şişman geçişi için kullanılır. |
| **Socket** | Modelin üzerinde tanımlı bağlantı noktası. "Şapka buraya takılır", "kutu bu ele gelir". |
| **Trigger Volume** | Görünmez kutu. İçine bir şey girdiğinde olay tetikler. Yükleme bölgesi için kullanılır. |
| **Lerp (yumuşak geçiş)** | Bir değeri A'dan B'ye pürüzsüzce taşımak. Kutunun rafa süzülerek yerleşmesi. |
| **Greybox / Blockout** | Oyunun kaba, gri kutulardan yapılmış prototip hâli. Sanat gelmeden oynanışı test etmek için. |
| **Placeholder** | Geçici asset. Sonradan gerçeği ile değişecek. |
| **Vertical Slice** | Oyunun küçük ama tam bitmiş bir dilimi. 15 dakikalık ama her sistemi çalışan bölüm. |
| **MVP** | "Minimum Viable Product". Fikri test etmeye yeten en küçük sürüm. |
| **Build** | Oyunun çalıştırılabilir hâline getirilmiş paketi. |
| **Regression Test** | Yeni değişikliğin eski çalışan şeyleri bozup bozmadığını kontrol etme. |
| **Profiling** | Oyunun neresi yavaş diye ölçüm yapmak. |
| **Seed (tohum)** | Rastgeleliği belirleyen sayı. Aynı tohum aynı rastgele sonuçları üretir. Test için hayati. |
| **Deterministic (belirlenimci)** | Aynı girdiyle her zaman aynı sonucu vermek. Kayıt yükleyince oyunun aynı şekilde devam etmesi. |
| **RNG** | Random Number Generator. Rastgele sayı üreteci. |
| **Save Versioning** | Kayıt dosyasına sürüm numarası koymak. Oyunu güncelleyince eski kayıtların bozulmaması için. |

---

## 2. Ekonomi ve Piyasa Terimleri

| Terim | Basit açıklama |
|---|---|
| **SKU** | "Stock Keeping Unit". Envanterdeki tek bir ürün çeşidi. "Su" bir SKU, "Enerji İçeceği" başka bir SKU. |
| **Baz fiyat (Base Price)** | Ürünün normal, olaysız günlerdeki referans fiyatı. Diğer her şey bunun katı olarak hesaplanır. |
| **Volatilite (oynaklık)** | Fiyatın ne kadar zıpladığı. Yüksek volatilite = fiyat çok değişiyor = hem büyük kazanç hem büyük kayıp ihtimali. |
| **Talep (Demand)** | İnsanların o üründen ne kadar istediği. |
| **Arz (Supply)** | Piyasada o üründen ne kadar bulunduğu. Arz düşer talep artarsa fiyat yükselir. |
| **Kıtlık (Scarcity)** | Arzın talebi karşılayamaması. Fiyat patlamasının ana sebebi. |
| **Marj (Margin)** | Satış fiyatı eksi maliyet. Kâr payı. "Yüksek marj" = az satıp çok kazanmak. |
| **Elastikiyet (Price Elasticity)** | Fiyat artınca talebin ne kadar düştüğü. Su elastik değildir (fiyat artsa da alırsınız), enerji içeceği elastiktir. |
| **Değer yoğunluğu (Value Density)** | Bir birim hacme kaç dolar sığdığı. Ekran kartı çok yoğun, çimento çok seyrektir. Bu oyunun en önemli karar ekseni. |
| **Ortalama maliyet (Average Cost)** | Elinizdeki malın size birim başına kaça mal olduğu. Kâr/zararı bununla karşılaştırırsınız. |
| **Ölü stok (Dead Stock)** | Satılamayan, elde kalan mal. Para bağlar, yer kaplar. |
| **Eskime (Obsolescence)** | Ürünün bozulmadan değer kaybetmesi. Elektronik ve moda ürünlerinde olur. |
| **Bozulma (Spoilage)** | Ürünün fiziken kullanılamaz hâle gelmesi. Gıdada olur. |
| **Soğuk zincir (Cold Chain)** | Ürünün baştan sona soğuk tutulması zorunluluğu. Zincir kırılırsa mal gider. |
| **Likidite** | Elinizdeki varlığı ne kadar hızlı nakde çevirebildiğiniz. Nakit en likit, depodaki 400 palet çimento en likit olmayan şeydir. |
| **Kaldıraç (Leverage)** | Borç alıp daha büyük işlem yapmak. Kazanırsanız çok kazanırsınız, kaybederseniz batarsınız. |
| **Exposure (maruziyet / risk tutarı)** | Şu anda kaç paranızın riskte olduğu. Depodaki malın değeri + ödenmemiş borç + kontrat cezası riski. |
| **Hedge (korunma)** | Bir riski dengelemek için ters yönde ikinci bir işlem yapmak. Sigorta gibi düşünün. |
| **Ortalamaya dönüş (Mean Reversion)** | Fiyatın zamanla normal seviyesine geri çekilme eğilimi. Uçan fiyat sonsuza kadar uçmaz. |
| **Darbe / İtki (Impulse)** | Bir olayın fiyata verdiği ani sıçrama. Sonra ortalamaya dönüş devreye girer. |
| **Arbitraj** | Aynı malı ucuz yerden alıp pahalı yere satmak. Oyunun temel kazanç yolu. |
| **Stokçuluk (Hoarding)** | Fiyat yükselsin diye malı satmayıp bekletmek. |
| **Fiyat fahişliği (Price Gouging)** | Kriz anında aşırı fiyat uygulamak. Kârlı ama dikkat çeker. |
| **Pazar payı (Market Share)** | Şehirdeki toplam arzın yüzde kaçının sizde olduğu. |
| **SLA** | "Service Level Agreement". Söz verilen teslimat şartları. Tutmazsanız ceza ödersiniz. |
| **Ceza (Penalty)** | Kontratı zamanında yerine getirememenin bedeli. |
| **Nakit akışı (Cash Flow)** | Kasaya giren ve çıkan paranın gün gün dengesi. Kârlı olup nakitsiz kalıp batabilirsiniz. |
| **Şirket değeri (Company Value)** | Nakit + malın satış değeri + ekipman − borç. Oyunun ana skor tabelası. |
| **Lead Time (tedarik süresi)** | Sipariş verdikten sonra malın gelmesi için geçen süre. |
| **Manifesto** | Bir kargonun içinde ne olduğunu beyan eden resmî liste. |
| **İrsaliye** | Sevk edilen malın yanında giden resmî belge. |
| **Bonded / Antrepo** | Gümrük vergisi henüz ödenmemiş, gözetim altındaki mal deposu. |
| **Excise / ÖTV** | Alkol, tütün, yakıt gibi mallara konan özel tüketim vergisi. Kaçakçılığın ana sebebi. |
| **Öncü madde (Precursor)** | Başka bir şeyin üretiminde kullanılabildiği için satışı takip edilen kimyasal. |
| **Hazmat** | "Hazardous Materials". Tehlikeli madde. Taşınması için özel izin gerekir. |

---

## 3. Bu Oyuna Özel Terimler

| Terim | Basit açıklama |
|---|---|
| **BU (Box Unit)** | Bu oyunun tek hacim ölçüsü. 1 BU = 1 orta boy karton kutu. Her araç, her raf, her ürün BU ile ölçülür. |
| **Kap (Container)** | Ürünün içinde durduğu fiziksel şey: Kutu S/M/L, Palet, Kasa, Soğuk Kap, Güvenli Kap. |
| **Beyaz işlem** | Faturalı, kayıtlı, vergisi ödenmiş alım-satım. Güvenli ama düşük marjlı. |
| **Gri işlem** | Faturasız alım-satım. Yüksek marjlı ama kayıt farkı yaratır. |
| **Stok Farkı (Discrepancy)** | Depodaki gerçek mal ile kayıtlarda görünen mal arasındaki fark. Denetimde bakılan ana şey. |
| **Şüphe (Heat)** | 0–100 arası bir sayaç. Gri işlem yaptıkça yükselir, temiz kaldıkça düşer. Denetim sıklığını belirler. |
| **Lisans / İzin** | Belirli ürün gruplarını satabilmek için gereken resmî belge. |
| **Manifestosuz Lot** | İçeriği bilinmeyen, mühürlü kargo. Müzayededen alınır, depoda açılır. |
| **Kanal (Channel)** | Ürünü nereden aldığınız: Tezgâh, Terminal, Tablet, Liman, Acil, Gri. |
| **Seyahat Et** | Aracınıza binip haritadan bir noktaya ışınlanma. Bu oyunda manuel araç kullanımı yoktur. |
| **Hedef Zinciri** | Kampanya bölümlerinin sandbox içindeki karşılığı. Sırayla açılan görev seti. |

---

## 4. Yayın ve Ticaret Terimleri

| Terim | Basit açıklama |
|---|---|
| **Early Access (EA)** | Oyunu bitmeden, oynanabilir hâlde satışa açmak. Oyuncu geri bildirimiyle büyütürsünüz. |
| **Wishlist** | Steam'de "istek listesine ekle". Steam'in görünürlük dağıtımında en önemli sayı. |
| **Steam Next Fest** | Steam'in yılda birkaç kez yaptığı demo festivali. En büyük wishlist sıçraması buradan gelir. |
| **Capsule** | Steam mağazasındaki kapak görseli. Küçük hâlinde tanınabilir olmalı. |
| **Content Update** | Çıkış sonrası eklenen ücretsiz içerik paketi. EA'nın motoru budur. |
| **Retention (elde tutma)** | Oyuncunun oyunu bırakmadan ne kadar oynadığı. |
| **Onboarding** | Oyuncunun ilk 20 dakikada oyunu öğrenme süreci. |
