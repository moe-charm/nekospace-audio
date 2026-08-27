import { mkdir, writeFile } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const engineUrl = process.env.VOICEVOX_URL ?? "http://127.0.0.1:50021";
const speaker = 56; // VOICEVOX:猫使アル / おちつき
const here = dirname(fileURLToPath(import.meta.url));
const outputDir = resolve(here, "..", "public", "narration-reverb");

const segments = [
  ["00-title.wav", "ネコスペース・リバーブ。"],
  [
    "00-intro.wav",
    "声や音楽に、自然な部屋の広がりを加えるリバーブです。まずは、6つのプリセットを聴き比べます。",
  ],
  ["01-booth.wav", "ボイスブース。短く明瞭で、声の輪郭を保ちます。"],
  ["02-rooms.wav", "木の小部屋、台詞向けステージ、柔らかな室内、広いホールへ切り替えます。"],
  ["03-body.wav", "テールだけの音と、初期反射を加えたルームボディを比較できます。"],
  ["04-early.wav", "イーアール・ソロでは、初期反射だけを確認できます。"],
  ["05-mono.wav", "モノ入力は、ウェット信号だけを中央にまとめ、ドライのステレオ感は残します。"],
  ["06-reset.wav", "リセットで、いつでも標準設定に戻せます。"],
];

const check = async (response, label) => {
  if (response.ok) return response;
  throw new Error(`${label} failed: ${response.status} ${await response.text()}`);
};

const speakersResponse = await check(await fetch(`${engineUrl}/speakers`), "speaker query");
const speakers = await speakersResponse.json();
const selectedStyle = speakers
  .flatMap((entry) => entry.styles.map((style) => ({ speaker: entry.name, ...style })))
  .find((style) => style.id === speaker);
if (!selectedStyle) throw new Error(`VOICEVOX speaker ID ${speaker} is not installed`);

await mkdir(outputDir, { recursive: true });
for (const [fileName, text] of segments) {
  const queryResponse = await check(
    await fetch(`${engineUrl}/audio_query?text=${encodeURIComponent(text)}&speaker=${speaker}`, {
      method: "POST",
    }),
    `audio_query for ${fileName}`,
  );
  const query = await queryResponse.json();
  Object.assign(query, {
    speedScale: 1.08,
    pitchScale: 0,
    intonationScale: 1,
    volumeScale: 1,
    prePhonemeLength: 0.12,
    postPhonemeLength: 0.16,
    outputSamplingRate: 48000,
    outputStereo: true,
  });
  const synthesisResponse = await check(
    await fetch(`${engineUrl}/synthesis?speaker=${speaker}`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(query),
    }),
    `synthesis for ${fileName}`,
  );
  await writeFile(resolve(outputDir, fileName), Buffer.from(await synthesisResponse.arrayBuffer()));
  console.log(`${fileName}: ${text}`);
}
console.log(`Generated ${segments.length} clips with VOICEVOX:${selectedStyle.speaker} (${selectedStyle.name})`);
