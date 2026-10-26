import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {pipeline} from 'node:stream/promises';
import {spawnSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const deps = path.join(root, '.deps');
fs.mkdirSync(deps, {recursive: true});
const archives = [
  {name: 'obs33-source.tar.gz', url: 'https://github.com/obsproject/obs-studio/releases/download/33.0.0/OBS-Studio-33.0.0-Sources.tar.gz',
    hash: 'c1ae3c98c1bb70ebdced33e894a526fcbbbda9bfd95806970a3fc31f74b08f6c', dest: deps,
    marker: path.join(deps, 'obs-studio-33.0.0-sources/libobs/obs.h')},
  {name: 'qt6.zip', url: 'https://github.com/obsproject/obs-deps/releases/download/2026-07-15/windows-deps-qt6-2026-07-15-x64.zip',
    hash: '7c7f985711d80467bdc1795b6592275a27d5b0e5a2c7a61db1f2c1d08d6a5579', dest: path.join(deps, 'qt6'),
    marker: path.join(deps, 'qt6/lib/cmake/Qt6/Qt6Config.cmake')},
];
for (const archive of archives) {
  const output = path.join(deps, archive.name);
  if (!fs.existsSync(output)) {
    console.log(`Downloading ${archive.name}`);
    const response = await fetch(archive.url);
    if (!response.ok) throw new Error(`${archive.url}: ${response.status}`);
    await pipeline(response.body, fs.createWriteStream(output + '.partial'));
    fs.renameSync(output + '.partial', output);
  }
  const digest = crypto.createHash('sha256');
  for await (const chunk of fs.createReadStream(output)) digest.update(chunk);
  if (digest.digest('hex') !== archive.hash) throw new Error(`SHA256 mismatch: ${output}`);
  if (!fs.existsSync(archive.marker)) {
    fs.mkdirSync(archive.dest, {recursive: true});
    const result = spawnSync('tar.exe', ['-xf', output, '-C', archive.dest, '--exclude=*/build-aux/run-*'], {stdio: 'inherit'});
    if (result.status !== 0) throw new Error(`Extraction failed: ${output}`);
  }
  console.log(`Verified ${archive.name}`);
}
