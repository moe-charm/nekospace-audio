import { mkdir, writeFile } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const engineUrl = process.env.VOICEVOX_URL ?? "http://127.0.0.1:50021";
const speaker = 56; // VOICEVOX:猫使アル / おちつき
const here = dirname(fileURLToPath(import.meta.url));
const outputDir = resolve(here, "..", "public", "narration");

const segments = [
  ["00-title.wav", "ネコスペース・バイノーラル。"],
  [
    "00-intro.wav",
    "モノラルの声や効果音を、ヘッドフォン向けの立体音響へ変換します。方位、高さ、距離を、画面上で直感的に動かせます。",
  ],
  ["01-left.wav", "音源を左へ動かすと、左耳側へ定位します。"],
  ["02-right.wav", "右へ動かすと、音像も右耳側へ移動します。"],
  ["03-above.wav", "エレベーションを上げると、上方向の音像を試せます。"],
  ["04-below.wav", "下げると、下方向の手がかりへ切り替わります。"],
  [
    "05-near.wav",
    "距離を近づけると、囁きや声を耳元へ近づけたような表現になります。",
  ],
];

const check = async (response, label) => {
  if (response.ok) return response;
  const detail = await response.text();
  throw new Error(`${label} failed: ${response.status} ${detail}`);
};

const speakersResponse = await check(
  await fetch(`${engineUrl}/speakers`),
  "VOICEVOX speaker query",
);
const speakers = await speakersResponse.json();
const selectedStyle = speakers
  .flatMap((entry) =>
    entry.styles.map((style) => ({ speaker: entry.name, ...style })),
  )
  .find((style) => style.id === speaker);

if (!selectedStyle) {
  throw new Error(`VOICEVOX speaker ID ${speaker} is not installed`);
}

await mkdir(outputDir, { recursive: true });

for (const [fileName, text] of segments) {
  const queryResponse = await check(
    await fetch(
      `${engineUrl}/audio_query?text=${encodeURIComponent(text)}&speaker=${speaker}`,
      { method: "POST" },
    ),
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

  const audio = Buffer.from(await synthesisResponse.arrayBuffer());
  await writeFile(resolve(outputDir, fileName), audio);
  console.log(`${fileName}: ${text}`);
}

console.log(
  `Generated ${segments.length} clips with VOICEVOX:${selectedStyle.speaker} (${selectedStyle.name})`,
);
