#!/usr/bin/env bash
set -e
cd "$(dirname "$0")"

# 1. Обновить soot до последнего main
git submodule update --remote --merge soot

# 2. Убрать артефакты сборки внутри soot, чтобы git не блокировал add
(cd soot && git checkout -- common/versions/revision.h 2>/dev/null || true)

# 3. Зафиксировать новый SHA submodule в родителе
git add -A soot
git commit -m "Bump soot to $(cd soot && git rev-parse --short HEAD)" || echo "soot already up to date"

# 4. Отправить
git push
