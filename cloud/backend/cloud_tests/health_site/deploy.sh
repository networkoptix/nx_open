#!/bin/bash

if [ "$1" = "prod" ]
then
    npm install
    ng build --prod
    aws s3 sync dist/ s3://nxcloud-prod-health-page-static/ --delete --cache-control 'max-age=86400,public'
    aws cloudfront create-invalidation --distribution-id EAKYCHFP0X5DM --paths "/assets/layout/tiles.json" --paths "/*"
elif [ "$1" = "prod-sla" ]
then
    npm install
    ng build --prod
    grep -rl https://api.status.nxvms.com dist | while read f
    do
        sed -i '' 's%https:\/\/api.status.nxvms.com%https://api.status-sla.nxvms.com%g' "$f"
    done

    aws s3 sync dist/ s3://nxcloud-prod-health-page-sla-static/ --delete --cache-control 'max-age=86400,public'
    aws cloudfront create-invalidation --distribution-id EN3ILZX13T4IB --paths "/assets/layout/tiles.json" --paths "/*"
else
    echo "Usage: deploy.sh <prod|prod-sla>"
fi
