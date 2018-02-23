const fsmv = require('fs-extra');
const fs = require('fs');
const path = require('path');
const copyCustomization = (customization) => {
	var customizationStyle = path.join(__dirname, '../customizations', customization, 'front_end/styles/_custom_palette.scss')
	var source = fs.createReadStream(customizationStyle);
	var dest = fs.createWriteStream(path.resolve(__dirname, 'app/styles/custom/_custom_palette.scss'));
	source.pipe(dest);

	source = path.resolve(__dirname, '../customizations', customization, 'front_end/images');
	dest = path.resolve(__dirname, 'app/images');
	fsmv.copy(source, dest, {mkdirp:true}, error => error ? console.log(error) : null);
}

copyCustomization(process.argv[3] || 'default');