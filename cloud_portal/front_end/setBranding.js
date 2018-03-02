const fsmv = require('fs-extra');
const fs = require('fs');
const path = require('path');
const copyCustomization = (customization) => {
	var customizationStyle = path.join('../customizations', customization, 'front_end/styles/_custom_palette.scss')
	var source = fs.createReadStream(customizationStyle);
	var dest = fs.createWriteStream('app/styles/custom/_custom_palette.scss');
	source.pipe(dest);

	source = path.resolve('../customizations', customization, 'front_end/images');
	dest = 'app/images';
	fsmv.copy(source, dest, {mkdirp:true}, error => error ? console.log(error) : null);
}

copyCustomization(process.argv[3] || 'default');