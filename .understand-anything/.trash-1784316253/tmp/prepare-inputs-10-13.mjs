import fs from 'node:fs';
import path from 'node:path';

const root = process.cwd();
const batchesPath = path.join(root, '.understand-anything', 'intermediate', 'batches.json');
const data = JSON.parse(fs.readFileSync(batchesPath, 'utf8'));

for (const batchIndex of [10, 11, 12, 13]) {
  const batch = data.batches.find((item) => item.batchIndex === batchIndex);
  if (!batch) throw new Error(`Missing batch ${batchIndex}`);
  const output = {
    projectRoot: root,
    batchFiles: batch.files,
    batchImportData: batch.batchImportData,
  };
  fs.writeFileSync(
    path.join(root, '.understand-anything', 'tmp', `ua-file-analyzer-input-${batchIndex}.json`),
    JSON.stringify(output, null, 2),
  );
}
