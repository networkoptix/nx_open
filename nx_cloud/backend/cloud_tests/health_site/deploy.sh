npm install
ng build --prod
aws s3 sync dist/ s3://nxcloud-prod-health-static/ --delete --cache-control 'max-age=86400,public'
aws cloudfront create-invalidation --distribution-id EAKYCHFP0X5DM --paths "/assets/layout/tiles.json" --paths "/*"
