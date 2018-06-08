ng build --prod
aws s3 sync dist/ s3://nxcloud-prod-health-static/ --delete --cache-control 'max-age=31536000,public'
aws cloudfront create-invalidation --distribution-id EAKYCHFP0X5DM --paths "/*"
